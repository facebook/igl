#!/usr/bin/env python3

import os
import shutil
from pathlib import Path

base_dir=os.getcwd()
lib_dir=os.path.join(base_dir, "src", "bc7enc")

shutil.copyfile(os.path.join(base_dir, "patches", "Compress.h"), os.path.join(lib_dir, "Compress.h"))
shutil.copyfile(os.path.join(base_dir, "patches", "Compress.cpp"), os.path.join(lib_dir, "Compress.cpp"))

file = Path(os.path.join(lib_dir, "CMakeLists.txt"))
file.write_text(file.read_text()
  .replace('-Wextra', '-Wextra -Wno-unused-function -Wno-unused-variable')
  .replace('VERSION 2.8', 'VERSION 3.16')
  .replace("test.cpp", "Compress.cpp")
  .replace("add_executable", "add_library"))
