# 构建与集成

[English](BUILD.md) · [首页](../README.zh-CN.md)

需要支持 C++20 的编译器、CMake 3.28+（使用 CMake 时）及 nlohmann/json；自动获取的 JSON 版本为 3.11.3。不依赖 Qt。

## 构建与测试
```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure --parallel
```
Windows 先进入 Visual Studio 开发者命令行；发现预览版使用 vswhere -prerelease。windows-debug/windows-release presets 包含安装规则和消费者测试。

独立构建默认启用 BUILD_TESTING、BUILD_CONFIG_EXAMPLES。BUILD_CONFIG_BENCHMARK 默认关闭，启用后获取 Google Benchmark。CONFIG_SANITIZER 开启 MSVC ASan 或 Unix ASan/UBSan；MSVC ASan 使用 RelWithDebInfo。ENABLE_COVERAGE 开启 Unix 覆盖率插桩。检测到 Python 3 时加入文档同步测试，CI 另行明确运行该检查。

## 宿主项目与离线构建

链接 config::config。依赖顺序为复用已有 nlohmann_json::nlohmann_json、查找包、按 CONFIG_FETCH_JSON 获取。设为 OFF 时必须预先提供 target 或安装包。

离线环境传入 FETCHCONTENT_SOURCE_DIR_JSON、FETCHCONTENT_SOURCE_DIR_GOOGLETEST；可选 benchmark 使用 FETCHCONTENT_SOURCE_DIR_GOOGLEBENCHMARK。开发警告和插桩选项仅作用于本项目可执行目标。

CONFIG_INSTALL 独立构建默认开启，嵌入宿主默认关闭。嵌入项目主动开启安装时，也必须提供可安装的 JSON 依赖，避免强迫宿主导出其私有 FetchContent target。

## 安装包
```sh
cmake -S . -B package -DBUILD_TESTING=OFF -DBUILD_CONFIG_EXAMPLES=OFF
cmake --install package --prefix install
```
默认获取的 JSON 随库安装。安装前缀包含公开和内部头、生成的 version.h、CMake 包文件、JSON 及 share/licenses/config/LICENSE.txt。外部提供 JSON 包时，消费方也需要能找到该包。
```cmake
find_package(config 0.2 CONFIG REQUIRED)
target_link_libraries(app PRIVATE config::config)
```
由于 0.2 调整 API，包兼容范围限制为相同次版本。

## 手动复制

复制 include/config，提供 nlohmann/json 头文件，启用 C++20 和平台线程支持（需要的工具链使用 -pthread）。Windows 使用 user_config_directory 时链接 Shell32.lib。config.hpp 不依赖生成的版本头，仅调用 version() 时需要另行复制生成的 version.h。

消费者测试覆盖安装包、已提供依赖的父项目 add_subdirectory、直接使用头文件三种方式；公共头独立编译和多翻译单元测试检查宏污染、遗漏 inline 等问题。
