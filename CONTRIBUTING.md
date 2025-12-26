# Contributing to Config Library

Thank you for your interest in contributing to the Config Library! We welcome contributions from everyone.

## How to Contribute

### 1. Reporting Bugs
If you find a bug, please open an issue on GitHub.
*   **Check existing issues** to see if the bug has already been reported.
*   **Provide a reproduction**: A minimal code example that demonstrates the issue is extremely helpful.
*   **Describe environment**: OS, Compiler version, CMake version, etc.

### 2. Suggesting Enhancements
We welcome ideas for new features or improvements.
*   Open an issue with the tag `enhancement`.
*   Clearly describe the feature and the problem it solves.

### 3. Pull Requests
1.  **Fork the repository** and create a new branch for your feature or fix.
2.  **Coding Style**:
    *   This project follows standard C++20 practices.
    *   We use `clang-format` to enforce code style. Please run `pre-commit run --all-files` before submitting.
    *   Ensure your code passes `clang-tidy` checks.
3.  **Tests**:
    *   Add unit tests for any new functionality.
    *   Ensure all existing tests pass (`ctest`).
4.  **Documentation**:
    *   Update `API_Reference.md` if you change public APIs.
    *   Add comments to your code where necessary.
5.  **Submit PR**:
    *   Target the `main` branch.
    *   Describe your changes in detail.
    *   Link to any related issues.

## Development Setup

1.  **Clone repo**: `git clone https://github.com/Hunlongyu/config.git`
2.  **Install dependencies**:
    *   C++20 Compiler
    *   CMake 3.28+
    *   Python (for pre-commit)
3.  **Setup hooks**: `pre-commit install`
4.  **Build**:
    ```bash
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
    cmake --build build
    ```

## License
By contributing, you agree that your contributions will be licensed under its MIT License.
