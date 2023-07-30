#!/usr/bin/env python3

import os
import shutil
from pathlib import Path

base_dir=os.getcwd()
lib_dir=os.path.join(base_dir, "src", "tinyobjloader")

file = Path(os.path.join(lib_dir, "CMakeLists.txt"))
file.write_text(file.read_text()
  .replace('VERSION 3.2', 'VERSION 3.5'))
