# 构建与安装指南

本指南提供了有关如何构建库示例、运行测试以及将库集成到项目中的说明。

##先决条件

*   **C++ 编译器**: 必须支持 **C++20** (例如 GCC 10+, Clang 10+, MSVC 19.29+)。
*   **CMake**: 版本 **3.28** 或更高。
*   **构建系统**: Ninja, Make 或 Visual Studio (取决于您的操作系统)。

## 源码构建

如果您想运行示例或测试，需要从源码构建项目。

### 1. 克隆仓库

```bash
git clone https://github.com/Hunlongyu/config.git
cd config
```

### 2. 配置项目

创建构建目录并运行 CMake 配置：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

**选项:**
*   `-DBUILD_CONFIG_EXAMPLES=ON`: 构建示例程序 (默认: ON)。
*   `-DBUILD_TESTING=ON`: 构建单元测试 (默认: ON)。
*   `-DBUILD_CONFIG_BENCHMARK=ON`: 构建基准测试工具 (默认: ON)。

### 3. 构建

```bash
cmake --build build --config Release
```

## 运行示例

构建完成后，示例可执行文件将位于 `build/examples/` 目录下。

```bash
# 示例: 运行基本用法示例
./build/examples/01_basic_usage
```

## 运行测试

要验证库在您的系统上是否正常工作，请运行测试套件：

```bash
cd build
ctest -C Release --output-on-failure
```

## 安装

由于这是一个 Header-only 库，“安装”仅仅意味着将头文件复制到标准位置。

```bash
cmake --install build --prefix /usr/local
```

这将把头文件复制到 `/usr/local/include/config/`。

## 集成

### 使用 `find_package` (安装后)

如果您已经全局或在本地安装了该库，可以在您的 `CMakeLists.txt` 中使用 `find_package`：

```cmake
find_package(config REQUIRED)
target_link_libraries(your_target PRIVATE config::config)
```
