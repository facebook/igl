#!/usr/bin/env python3

import os
import shutil
from pathlib import Path

base_dir=os.getcwd()
lib_dir=os.path.join(base_dir, "src", "glslang")

file = Path(os.path.join(lib_dir, "parse_version.cmake"))
file.write_text(file.read_text()
  .replace('function(parse_version FILE PROJECT)', '''
function(parse_version FILE PROJECT)
    message("FILE   : " ${PROJECT})
    message("PROJECT: " ${PROJECT})
    set("${PROJECT}_VERSION_MAJOR"  "12" PARENT_SCOPE)
    set("${PROJECT}_VERSION_MINOR"  "1"  PARENT_SCOPE)
    set("${PROJECT}_VERSION_PATCH"  "0"  PARENT_SCOPE)
    set("${PROJECT}_VERSION_FLAVOR" ""   PARENT_SCOPE)
    set("${PROJECT}_VERSION" "12.1.0" PARENT_SCOPE)
endfunction()

function(parse_version_old FILE PROJECT)'''))
