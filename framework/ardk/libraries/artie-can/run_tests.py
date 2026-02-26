#!/usr/bin/env python3
"""
Test runner script for Artie CAN library unit tests.

This script provides a convenient way to run the test suite with common options.
"""
import sys
import subprocess
from pathlib import Path


def main():
    """Run pytest with appropriate arguments."""

    # Get the directory containing this script
    script_dir = Path(__file__).parent.resolve()
    test_dir = script_dir / "tests"

    # Check if pytest is available
    try:
        import pytest
    except ImportError:
        print("ERROR: pytest not found. Please install development dependencies:")
        print("  pip install -e \".[dev]\"")
        return 1

    # Default pytest arguments
    pytest_args = [
        str(test_dir),
        "-v",
        "--tb=short",
    ]

    # Add any command line arguments passed to this script
    if len(sys.argv) > 1:
        pytest_args.extend(sys.argv[1:])

    # Run pytest
    print(f"Running tests in {test_dir}")
    print(f"Command: pytest {' '.join(pytest_args)}\n")

    return pytest.main(pytest_args)


if __name__ == "__main__":
    sys.exit(main())
