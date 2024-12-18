Our [contribution guidelines](https://github.com/facebook/igl/blob/main/CONTRIBUTING.md):

## Pull Requests
We actively welcome your pull requests. Here's the procedure to submit pull requests to IGL:

1. **Submit an issue describing your proposed changes.**
2. The repo owner will respond to your issue.
3. If your proposed changes are accepted, fork the repo and develop & test your changes.
4. If you've added code that should be tested, add tests (and ensure they do pass).
5. Verify your changes work as expected on all relevant rendering backends, and also test them when any combination of backends is disabled using the `IGL_WITH_*` CMake options.
6. If you've changed APIs, update the documentation.
7. Make sure your code lints (run `clang-format` or use `.clang-format` in Visual Studio).
8. If you haven't already, complete the Contributor License Agreement ("CLA").
9. Please respect `// @fb-only` comments and do not delete them!
10. Open a pull request.

## Dependencies
One of our design objectives is that IGL itself should have as few dependencies as possible.
No new third-party dependencies will be accepted unless they are absolutely critical to the core functionality of IGL.

**Please include a link to the related (and accepted) GitHub issue in this PR!**
