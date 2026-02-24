from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
import subprocess
import os
import shutil
import sys

class CMakeBuild(build_ext):
    """Custom build extension to build C library with CMake"""

    def run(self):
        # Check if CMake is available
        try:
            subprocess.check_output(['cmake', '--version'])
        except OSError:
            raise RuntimeError("CMake must be installed to build artie-can")

        # Build C library with CMake
        build_dir = os.path.join(self.build_temp, 'cmake_build')
        os.makedirs(build_dir, exist_ok=True)

        # Configure
        cmake_args = [
            f'-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={os.path.abspath(self.build_lib)}/artie_can/lib',
            f'-DCMAKE_BUILD_TYPE=Release',
            '-DBUILD_SHARED_LIBS=ON',
            '-DBUILD_STATIC_LIBS=OFF',
            '-DBUILD_TESTS=OFF',
        ]

        configure_cmd = ['cmake', '..'] + cmake_args
        subprocess.check_call(configure_cmd, cwd=build_dir)

        # Build
        build_cmd = ['cmake', '--build', '.', '--config', 'Release']
        subprocess.check_call(build_cmd, cwd=build_dir)

        # Copy shared library to package
        lib_dir = os.path.join(self.build_lib, 'artie_can', 'lib')
        os.makedirs(lib_dir, exist_ok=True)

        # Find and copy the built library
        for filename in os.listdir(build_dir):
            if filename.startswith('libartie_can') and (filename.endswith('.so') or filename.endswith('.dylib')):
                src = os.path.join(build_dir, filename)
                dst = os.path.join(lib_dir, filename)
                shutil.copy(src, dst)
                print(f"Copied {src} to {dst}")

setup(
    name='artie-can',
    version="0.1.0",
    python_requires=">=3.10",
    license="MIT",
    description="Artie CAN library for communication over CAN bus",
    packages=["artie_can"],
    package_dir={"artie_can": "src/artie_can"},
    package_data={"artie_can": ["lib/*.so", "lib/*.dylib"]},
    include_package_data=True,
    ext_modules=[Extension('artie_can._dummy', [])],  # Dummy extension to trigger build_ext
    cmdclass={'build_ext': CMakeBuild},
    install_requires=[],
    extras_require={
        "dev": [
            "pytest>=7.0",
        ],
    },
)
