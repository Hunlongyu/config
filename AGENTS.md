# 工程约定

- C++20、header-only、nlohmann/json。主 API 为 `config::Store`，可选监视器独立持有。
- 修改行为前阅读 `docs/API.zh-CN.md`、`docs/ARCHITECTURE.zh-CN.md`；公开 API 变化同步中英文文档和迁移表。
- 文件测试必须使用 `tests/test_support.hpp` 的独立临时目录，不读取或删除真实 AppData 配置。
- 自动修改需先保存再发布；失败不得修改已提交状态或发出成功事件。用户回调不在内部锁下执行。
- 不自动提交或推送。提交信息使用中文 Conventional Commits。

## 构建与验证

先初始化 MSVC。通过 `vswhere.exe -latest -prerelease -products * -property installationPath` 发现安装目录，再调用其 `VC/Auxiliary/Build/vcvars64.bat`。

本机已验证的安装根为 `C:/Program Files/Microsoft Visual Studio/18/Insiders`；CMake 位于 `Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin`。不固定 `VC/Tools/MSVC` 的版本号。

```text
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 6
ctest --test-dir build --output-on-failure --parallel 6
python scripts/check_docs.py
```

- 离线依赖可通过 `FETCHCONTENT_SOURCE_DIR_JSON`、`FETCHCONTENT_SOURCE_DIR_GOOGLETEST` 指定。
- 修改公共头后，确认 Ninja 增量重编译；`cmake/DetectMsvcIncludes.cmake` 处理本机 UTF-8 中文头文件依赖输出。
- 保存串行锁的变异测试必须成功拒绝故障副本；消费者和示例测试同样属于验收。
- Windows ASan 使用独立构建目录、`RelWithDebInfo` 和 `CONFIG_SANITIZER=ON`。
- 本地测试结果与尚未执行的跨平台 CI 结果分开报告。
