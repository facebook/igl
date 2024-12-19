#!/usr/bin/env python3

import os
import shutil
from pathlib import Path

base_dir=os.getcwd()
lib_dir=os.path.join(base_dir, "src", "nanovg")

file = Path(os.path.join(lib_dir, "example/demo.c"))
file.write_text(file.read_text()
  .replace("#include <GLFW/glfw3.h>", "#include <stdlib.h>")
  .replace("#define STB_IMAGE_WRITE_IMPLEMENTATION", "")
  .replace("glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, image);", "// glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, image);")
)

file = Path(os.path.join(lib_dir, "example/perf.c"))
file.write_text(file.read_text()
  .replace("#include <GLFW/glfw3.h>", "")
  .replace("void startGPUTimer(GPUtimer* timer)",
("""#if 0
void startGPUTimer(GPUtimer* timer)
"""))
  .replace("void initGraph(PerfGraph* fps, int style, const char* name)",
("""#endif

void initGraph(PerfGraph* fps, int style, const char* name)
"""))
)


