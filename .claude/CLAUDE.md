# IGL Project Instructions

## Diff Formatting

- Use Markdown formatting in diff summaries, titles, and test plans.
- Diff titles should start with the relevant backend prefix, e.g., `igl | vulkan |`, `igl | metal |`, `igl | opengl |`. Use `igl | cmake |` for CMake-related changes, `igl | github |` for purely open-source related changes, and `igl |` alone for changes not specific to a single backend.
- Always add trailing `()` to function and method names mentioned in titles, summaries, test plans, and actual code comments (e.g., `createInstance()`, `enable()`). Do not add `()` to type or class names (e.g., `RenderPipelineState`, `TextureDesc`).

## Coding Style

- Use C++20 designated initializers whenever possible.
- For NESTED designated initializers that fit on a single line, omit the trailing comma.
- Prefer `const` for local variables whenever possible. Be aggressive, i.e., the structure `RenderPipelineDesc` can be initialized like that with all the nested fields, including attachments.
