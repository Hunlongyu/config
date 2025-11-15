# TODO（标准 C++ 跨平台库完善清单）

仅列未完成项；按优先级排序。完成后勾选对应项。

## 高优先级
- [x] 在 CMake 统一版本管理并生成可查询的 `version.h`
- [ ] 建立 GitHub Actions CI 构建矩阵（Windows/MSVC、Linux/GCC、macOS/Clang），启用缓存与并行
- [ ] 引入单元测试框架（GoogleTest 或 Catch2），集成 CMake/ctest，覆盖核心功能与边界条件
- [ ] 配置代码覆盖率（llvm-cov/lcov + Codecov/Coveralls 上传），在 CI 中生成与上报
- [ ] 增加英文版 README，与中文保持结构一致，在主页提供语言切换链接
- [ ] 添加 CONTRIBUTING.md 与 CODE_OF_CONDUCT.md，明确分支策略、提交规范与评审流程
- [ ] 添加 Issue/PR 模板（Bug/Feature/Question），提升问题与需求收集质量
- [ ] 统一编译标准与警告级别（设置 CMAKE_CXX_STANDARD；Windows `/W4`、Linux/macOS `-Wall -Wextra`），提供可选 `-Werror` 开关

## 中优先级
- [ ] 增加 UNIX/APPLE 平台分支与配置：路径/编码处理、符号可见性、导出宏（`__declspec(dllexport)`/`-fvisibility=hidden`）
- [ ] 配置 clang-tidy 与 pre-commit 钩子，统一静态检查与格式化工作流
- [ ] 引入 Sanitizers（ASan/UBSan/Tsan），在 CI 中执行相应任务以提前发现问题
- [ ] 生成并发布 API 文档（Doxygen），通过 GitHub Pages 或 Releases 提供浏览
- [ ] 维护 CHANGELOG.md，遵循 SemVer；在发布中附带变更摘要与升级注意事项
- [ ] 添加 EditorConfig，统一缩进、编码与行尾风格
- [ ] 增加 benchmark 目录与基准测试（如 Google Benchmark），覆盖关键算法与接口性能

## 低优先级
- [ ] 集成包管理器清单（Conan 或 vcpkg），提供一键安装与可复用依赖配置
- [ ] 增加安全策略与漏洞报告流程（SECURITY.md），并在仓库启用 CodeQL 安全扫描
- [ ] 发布产物自动化：打包各平台制品（Windows/Linux/macOS）并随 GitHub Releases 发布
- [ ] 明确兼容性矩阵与支持策略（最低 C++ 标准、受支持编译器版本），在文档中列出
- [ ] 为 `examples/` 添加示例说明与运行指引，提升上手体验
- [ ] 依赖许可合规清单与第三方版权声明，确保发布合规

## 备注
- 已存在项（CMake 构建与安装导出、示例、.clang-format、LICENSE、中文 README）不在本清单中重复列出