#!/usr/bin/env python3
"""Verify a runtime package on a build host, without loading any modules."""
import argparse
import importlib.util
from pathlib import Path

spec = importlib.util.spec_from_file_location(
    'runtime_device', Path(__file__).with_name('runtime-device.py'))
runtime = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runtime)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('package', type=Path)
    args = parser.parse_args()
    print(f'PASS: {len(runtime.checked_package(args.package.resolve()))} runtime payload files')
