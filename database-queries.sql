-- ESP32 Irrigation Controller - Database Queries
-- Use these queries to analyze your irrigation data

-- ===========================================
-- DEVICE INFORMATION QUERIES
-- ===========================================

-- Get current device configuration
SELECT
    device_id,
    ip_address,
    mac_address,
    wifi_ssid,
    wifi_rssi,
    firmware_version,
    timezone,
    max_zones,
    datetime(timestamp) as last_update
FROM device_config
ORDER BY timestamp DESC
LIMIT 1;

-- Device uptime history
SELECT
    ip_address,
    uptime,
    heap_free,
    wifi_rssi,
    datetime(timestamp) as recorded_at
FROM device_config
ORDER BY timestamp DESC
LIMIT 20;

-- ===========================================
-- ZONE ACTIVITY ANALYSIS
-- ===========================================

-- Recent zone activity (last 24 hours)
SELECT
    zone_number,
    status,
    time_remaining,
    datetime(timestamp) as activity_time
FROM zone_status
WHERE datetime(timestamp) > datetime('now', '-1 day')
ORDER BY timestamp DESC;

-- Zone usage summary by day
SELECT
    date(timestamp) as irrigation_date,
    zone_number,
    COUNT(*) as activations,
    SUM(CASE WHEN time_remaining > 0 THEN time_remaining ELSE 0 END) as total_runtime_seconds
FROM zone_status
WHERE status = 'RUNNING'
GROUP BY date(timestamp), zone_number
ORDER BY irrigation_date DESC, zone_number;

-- Most active zones
SELECT
    zone_number,
    COUNT(*) as total_activations,
    COUNT(CASE WHEN date(timestamp) = date('now') THEN 1 END) as today_activations
FROM zone_status
WHERE status = 'RUNNING'
GROUP BY zone_number
ORDER BY total_activations DESC;

-- ===========================================
-- IRRIGATION SCHEDULING ANALYSIS
-- ===========================================

-- Daily irrigation patterns
SELECT
    date(timestamp) as irrigation_date,
    COUNT(DISTINCT zone_number) as zones_watered,
    COUNT(*) as total_activations,
    MIN(time(timestamp)) as first_watering,
    MAX(time(timestamp)) as last_watering
FROM zone_status
WHERE status = 'RUNNING'
GROUP BY date(timestamp)
ORDER BY irrigation_date DESC;

-- Hourly irrigation distribution
SELECT
    strftime('%H', timestamp) as hour_of_day,
    COUNT(*) as activations,
    COUNT(DISTINCT zone_number) as unique_zones
FROM zone_status
WHERE status = 'RUNNING'
    AND date(timestamp) > date('now', '-7 days')
GROUP BY strftime('%H', timestamp)
ORDER BY hour_of_day;

-- ===========================================
-- SYSTEM HEALTH MONITORING
-- ===========================================

-- WiFi signal strength over time
SELECT
    datetime(timestamp) as recorded_at,
    wifi_rssi,
    CASE
        WHEN wifi_rssi > -50 THEN 'Excellent'
        WHEN wifi_rssi > -60 THEN 'Good'
        WHEN wifi_rssi > -70 THEN 'Fair'
        ELSE 'Poor'
    END as signal_quality
FROM device_config
WHERE wifi_rssi IS NOT NULL
ORDER BY timestamp DESC
LIMIT 50;

-- Memory usage trends
SELECT
    datetime(timestamp) as recorded_at,
    heap_free,
    uptime,
    ROUND(heap_free / 1024.0, 2) as heap_free_kb
FROM device_config
WHERE heap_free IS NOT NULL
ORDER BY timestamp DESC
LIMIT 50;

-- System reboot detection (uptime decreases)
SELECT
    datetime(timestamp) as reboot_time,
    uptime,
    LAG(uptime) OVER (ORDER BY timestamp) as previous_uptime
FROM device_config
WHERE uptime < LAG(uptime) OVER (ORDER BY timestamp)
ORDER BY timestamp DESC;

-- ===========================================
-- ERROR AND ISSUE DETECTION
-- ===========================================

-- Find zones that failed to start
SELECT
    zone_number,
    timestamp,
    status
FROM zone_status
WHERE status IN ('ERROR', 'FAILED', 'TIMEOUT')
ORDER BY timestamp DESC;

-- Detect rapid zone status changes (potential issues)
SELECT
    zs1.zone_number,
    zs1.status as status1,
    zs2.status as status2,
    datetime(zs1.timestamp) as time1,
    datetime(zs2.timestamp) as time2,
    (julianday(zs2.timestamp) - julianday(zs1.timestamp)) * 86400 as seconds_between
FROM zone_status zs1
JOIN zone_status zs2 ON zs1.zone_number = zs2.zone_number
WHERE zs2.timestamp > zs1.timestamp
    AND (julianday(zs2.timestamp) - julianday(zs1.timestamp)) * 86400 < 30
    AND zs1.status != zs2.status
ORDER BY zs1.timestamp DESC;

-- ===========================================
-- REPORTING QUERIES
-- ===========================================

-- Weekly irrigation summary
SELECT
    strftime('%Y-W%W', timestamp) as week,
    COUNT(DISTINCT date(timestamp)) as days_with_irrigation,
    COUNT(*) as total_zone_activations,
    COUNT(DISTINCT zone_number) as zones_used,
    SUM(time_remaining) / 60.0 as total_runtime_minutes
FROM zone_status
WHERE status = 'RUNNING'
    AND timestamp > datetime('now', '-4 weeks')
GROUP BY strftime('%Y-W%W', timestamp)
ORDER BY week DESC;

-- Monthly water usage estimate (assuming 1 minute = X gallons per zone)
SELECT
    strftime('%Y-%m', timestamp) as month,
    zone_number,
    COUNT(*) as activations,
    SUM(time_remaining) / 60.0 as runtime_minutes,
    -- Estimate gallons (adjust multiplier based on your sprinkler flow rate)
    ROUND(SUM(time_remaining) / 60.0 * 2.5, 1) as estimated_gallons
FROM zone_status
WHERE status = 'RUNNING'
    AND time_remaining > 0
GROUP BY strftime('%Y-%m', timestamp), zone_number
ORDER BY month DESC, zone_number;

-- ===========================================
-- MAINTENANCE QUERIES
-- ===========================================

-- Database size and record counts
SELECT
    'device_config' as table_name,
    COUNT(*) as record_count
FROM device_config
UNION ALL
SELECT
    'device_status' as table_name,
    COUNT(*) as record_count
FROM device_status
UNION ALL
SELECT
    'zone_status' as table_name,
    COUNT(*) as record_count
FROM zone_status
UNION ALL
SELECT
    'schedule_status' as table_name,
    COUNT(*) as record_count
FROM schedule_status;

-- Cleanup old records (run manually when needed)
-- Uncomment and modify retention period as needed

/*
-- Delete device status older than 30 days
DELETE FROM device_status
WHERE timestamp < datetime('now', '-30 days');

-- Delete zone status older than 90 days
DELETE FROM zone_status
WHERE timestamp < datetime('now', '-90 days');

-- Keep only latest device config per day
DELETE FROM device_config
WHERE id NOT IN (
    SELECT MAX(id)
    FROM device_config
    GROUP BY date(timestamp)
);
*/