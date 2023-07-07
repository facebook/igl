#!/usr/bin/python3
# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.


import os
import sys

folder = "third-party"
script = os.path.join(folder, "bootstrap.py")
json = os.path.join(folder, "bootstrap-deps.json")
base = os.path.join(folder, "deps")

os.system(
    '"{}" {} -b {} --bootstrap-file={} --break-on-first-error'.format(
        sys.executable, script, base, json
    )
)
