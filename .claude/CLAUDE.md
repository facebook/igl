# IGL Project Instructions

## Diff Formatting

- Use Markdown formatting in diff summaries, titles, and test plans.
- Diff titles should start with the relevant backend prefix, e.g., `igl | vulkan |`, `igl | metal |`, `igl | opengl |`. Use `igl | cmake |` for CMake-related changes, `igl | github |` for purely open-source related changes, and `igl |` alone for changes not specific to a single backend. Capitalize the first letter after the last `|` separator.
- Always add trailing `()` to function and method names mentioned in titles, summaries, test plans, and actual code comments (e.g., `createInstance()`, `enable()`). Do not add `()` to type or class names (e.g., `RenderPipelineState`, `TextureDesc`).

## Coding Style

- Use C++20 designated initializers whenever possible.
- For NESTED designated initializers that fit on a single line, omit the trailing comma.
- Prefer `const` for local variables whenever possible. Be aggressive, i.e., the structure `RenderPipelineDesc` can be initialized like that with all the nested fields, including attachments.
- Don't use `auto` for short scalar types (int, size_t, uint8_t, float, etc.).

### Thread Safety

- Place `IGL_ENSURE_VULKAN_CONTEXT_THREAD(ctx_)` at the start of methods that access Vulkan device state. This verifies the call is on the correct thread.

### Profiling

- Add `IGL_PROFILER_FUNCTION()` or `IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE)` at the start of public/significant methods.
- Use `IGL_PROFILER_COLOR_CREATE` for resource creation, `IGL_PROFILER_COLOR_DESTROY` for destruction, `IGL_PROFILER_COLOR_WAIT` for waits/syncs, `IGL_PROFILER_COLOR_SUBMIT` for command submission.
- Use `IGL_PROFILER_ZONE("name", color)` / `IGL_PROFILER_ZONE_END()` for scoped profiling within a function.

### Debug Names

- All Vulkan objects (images, buffers, pipelines, command pools, fences, semaphores) should be given debug names via `ivkSetDebugObjectName()`.
- Use `IGL_FORMAT("descriptive name: {}", identifier)` for formatting debug name strings.
- C-style cast `(uint64_t)vkHandle` when passing Vulkan handles to `ivkSetDebugObjectName()`.

### Deferred Destruction

- Vulkan GPU resources that may still be in flight must be destroyed via deferred tasks (capture all needed values by value in the lambda - never capture `this` or references to the dying object):

```cpp
ctx_.deferredTask(std::packaged_task<void()>(
    [vf = &ctx_.vf_, device = device_, buffer = vkBuffer_]() {
      vf->vkDestroyBuffer(device, buffer, nullptr);
    }));
```

### Pointer Annotations

- Use `IGL_NULLABLE` on pointer parameters and return types that may be null.
- Use `IGL_NONNULL` on pointer parameters that must not be null.
- Pattern for optional output result parameters: `Result* IGL_NULLABLE outResult`.
