# Contributing

Use the C++20 style in .clang-format. Add behavior-focused tests for changes, and use unique temporary directories for file tests.

Build and run tests as described in [BUILD](docs/BUILD.md). The regression suites, consumer tests, compiled examples and save-lock mutation guard must pass. Run `python scripts/check_docs.py` after documentation changes; `--update` refreshes marked snippets from examples.

Keep [English](README.md) and [Chinese](README.zh-CN.md) documentation aligned. API changes require updates to both API references, migration notes and CHANGELOG.md. Do not claim cross-platform runtime verification from a Windows-only run.

Commit messages use Chinese Conventional Commits. Bug reports should include a minimal reproduction, operating system, compiler and the relevant Error code. Contributions use the [MIT license](LICENSE.txt).
