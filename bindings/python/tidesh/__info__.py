"""Stores information on the current module version"""

import datetime
import typing

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

__copyright__ = f"Copyright {__year__}, {__module__}"

__raw_version__ = "1.0"
__version__ = "1.0-5cf7ee4"
__git_version__ = "5cf7ee4"
__build_date__ = datetime.datetime.strptime("2026-02-18", "%Y-%m-%d")
__build_type__: typing.Literal["debug", "release"] = "release"
__platform__: typing.Literal["linux", "macos", "windows"] = "macos"
__brief__ = "A lightweight but fully-featured shell"

from ._tidesh import ffi, lib

__compiler__ = ffi.string(lib.tidesh_compiler).decode()
