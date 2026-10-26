"""Setup script for tidesh Python bindings using setuptools with CFFI."""

import os
from pathlib import Path

from cffi import FFI
from setuptools import setup

# Get macros from environment variables (passed from Makefile)
PROJECT_NAME = os.environ.get("PROJECT_NAME", "tidesh")
VERSION = os.environ.get("RAW_VERSION", "1.0")
BRIEF = os.environ.get("BRIEF", "A lightweight but fully-featured shell")
PLATFORM = os.environ.get("PLATFORM", "unknown")
GIT_VERSION = os.environ.get("GIT_VERSION", "unknown")
BUILD_DATE = os.environ.get("BUILD_DATE")
if not BUILD_DATE or BUILD_DATE == "unknown":
    import datetime

    BUILD_DATE = datetime.date.today().strftime("%Y-%m-%d")
BUILD_TYPE = os.environ.get("BUILD_TYPE", "release")

FULL_VERSION = f"{VERSION}-{GIT_VERSION}"

# Generate __info__.py
info_path = Path(__file__).parent / "tidesh" / "__info__.py"
info_content = f'''
"""Stores information on the current module version"""
import typing
import datetime

# Authors
__author__ = "Anime no Sekai"
__maintainer__ = "Anime no Sekai"
__credits__ = ["animenosekai"]
__email__ = "animenosekai.mail@gmail.com"
__repository__ = "https://github.com/Animenosekai/tidesh"

# Module
__module__ = "tidesh"
__status__ = "Beta"
__year__ = 2026
__license__ = "MIT License"

__copyright__ = f"Copyright {{__year__}}, {{__module__}}"

__raw_version__ = "{VERSION}"
__version__ = "{FULL_VERSION}"
__git_version__ = "{GIT_VERSION}"
__build_date__ = datetime.datetime.strptime("{BUILD_DATE}", "%Y-%m-%d")
__build_type__: typing.Literal["debug", "release"] = "{BUILD_TYPE}"
__platform__: typing.Literal["linux", "macos", "windows"] = "{PLATFORM}"
__brief__ = "{BRIEF}"

from ._tidesh import ffi, lib
__compiler__ = ffi.string(lib.tidesh_compiler).decode()

'''
info_path.write_text(info_content)

# Initialize FFI
ffibuilder = FFI()
root_path = Path(__file__).parent

# Read the CFFI interface definition
cdef_path = root_path / "tidesh" / "tidesh.cdef"
ffibuilder.cdef(cdef_path.read_text())

# Try to find C sources in multiple locations:
# 1. Staged locally (when building from Makefile or sdist)
# 2. Original repo root (when building directly from git)
root_path = Path(__file__).parent

# First check if staged locally
src_root = (root_path / "src").resolve()
include_root = (root_path / "include").resolve()

# Fallback to repo root
if not src_root.exists():
    src_root = (root_path.parent.parent / "src").resolve()
    include_root = (root_path.parent.parent / "include").resolve()

if not src_root.exists():
    msg = f"""C source directory not found.

Tried to find sources at:
  1. {root_path / "src"} (staged locally)
  2. {root_path.parent.parent / "src"} (repo root)

This typically happens when building outside of the project.
The Python bindings should be built from the repository root using:
    make build/python

Or ensure sources are staged in bindings/python/src before building.
"""
    raise FileNotFoundError(msg)

sources = [
    str(p.relative_to(src_root))
    for p in sorted(src_root.glob("**/*.c"))
    if p.name not in ("main.c", "preload.c", "sandbox_preload.c")
]

# Setuptools requires source and include paths to be relative to setup.py.
source_files = [os.path.relpath(src_root / s, root_path) for s in sources]
include_dirs = [os.path.relpath(include_root, root_path)]

# Read the source glue
source_glue_path = root_path / "tidesh" / "tidesh.c"
source_glue = source_glue_path.read_text()

# Platform-specific compile flags
extra_compile_args: list[str] = ["-O3"]
if PLATFORM == "linux":
    extra_compile_args.append("-D_GNU_SOURCE")
elif PLATFORM == "macos":
    extra_compile_args.append("-D_XOPEN_SOURCE=700")
    extra_compile_args.append("-D_DARWIN_C_SOURCE")

# Configure the CFFI source
ffibuilder.set_source(
    "tidesh._tidesh",
    source_glue,
    sources=source_files,
    include_dirs=include_dirs,
    define_macros=[
        ("PROJECT_NAME", f'"{PROJECT_NAME}"'),
        ("RAW_VERSION", f'"{VERSION}"'),
        ("GIT_VERSION", f'"{GIT_VERSION}"'),
        ("VERSION", f'"{FULL_VERSION}"'),
        ("BUILD_DATE", f'"{BUILD_DATE}"'),
        ("PLATFORM", f'"{PLATFORM}"'),
        ("BRIEF", f'"{BRIEF}"'),
    ],
    extra_compile_args=extra_compile_args,
)

# Build the extension module
ext_modules = [ffibuilder.distutils_extension()]

# Call setuptools.setup
setup(ext_modules=ext_modules)
