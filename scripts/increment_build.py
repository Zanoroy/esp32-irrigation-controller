#!/usr/bin/env python3
"""
Pre-build script to auto-increment build number
Run by PlatformIO before each build
"""
import os

Import("env")

def increment_build_number():
    # Get the project directory from PlatformIO environment
    project_dir = env.get("PROJECT_DIR")
    build_file = os.path.join(project_dir, 'include', 'build_number.h')

    # Read current build number
    current_build = 1
    if os.path.exists(build_file):
        with open(build_file, 'r') as f:
            for line in f:
                if 'BUILD_NUMBER' in line and '#define' in line:
                    try:
                        current_build = int(line.split()[-1])
                    except (ValueError, IndexError):
                        pass

    # Increment
    new_build = current_build + 1

    # Write new build number
    with open(build_file, 'w') as f:
        f.write('#ifndef BUILD_NUMBER_H\n')
        f.write('#define BUILD_NUMBER_H\n')
        f.write('\n')
        f.write(f'#define BUILD_NUMBER {new_build}\n')
        f.write('\n')
        f.write('#endif // BUILD_NUMBER_H\n')

    print(f'Build number incremented: {current_build} -> {new_build}')

# Call the function when PlatformIO runs this script
increment_build_number()
