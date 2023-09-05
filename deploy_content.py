#!/usr/bin/python3
# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.


import os
import sys

folder = "third-party"
script = os.path.join(folder, "bootstrap.py")
json = os.path.join(folder, "bootstrap-content.json")
base = os.path.join(folder, "content")

try:
    os.mkdir(base)
except FileExistsError:
    pass

os.system(
    '"{}" {} -b {} --bootstrap-file={}'.format(sys.executable, script, base, json)
)
