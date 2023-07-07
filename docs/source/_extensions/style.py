from pygments.filters import VisibleWhitespaceFilter
from pygments.lexers.compiled import CppLexer, RustLexer
from pygments.lexers.make import CMakeLexer
from sphinx.highlighting import lexers


def setup(app):
    """Replace tabs with 4 spaces"""
    lexers["C++"] = CppLexer()
    lexers["rust"] = RustLexer()
    lexers["CMake"] = CMakeLexer()

    ws_filter = VisibleWhitespaceFilter(tabs=" ", tabsize=4)
    for lx in lexers.values():
        lx.add_filter(ws_filter)
