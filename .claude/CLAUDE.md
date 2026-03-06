# IGL Project Instructions

## Diff Formatting

- Use Markdown formatting in diff summaries, titles, and test plans.
- Diff titles should start with the relevant backend prefix, e.g., `igl | vulkan |`, `igl | metal |`, `igl | opengl |`. Use `igl | cmake |` for CMake-related changes, `igl | github |` for purely open-source related changes, and `igl |` alone for changes not specific to a single backend.
- Always add trailing `()` to function and method names mentioned in titles, summaries, test plans, and actual code comments (e.g., `createInstance()`, `enable()`).

## Coding Style

- Use C++20 designated initializers whenever possible.
- When initializing local variables, if nested designated initializers can fit on one line, don't use a trailing comma for NESTED designated initializers.
- Prefer `const` for local variables whenever possible.
