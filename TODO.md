# TODO List

## 🚀 Future Features

### 1. Support Direct JSON-to-Struct Serialization (JSON 直接转换为结构体)

**Status**: Planned  
**Priority**: High

#### 说明 (Explanation)
利用底层 `nlohmann::json` 库的特性，支持将 JSON 配置直接反序列化为 C++ 结构体。
- **机制**: 用户定义结构体并使用 `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE` 宏进行绑定。
- **用法**: `store.get<MyStruct>("")` 将整个配置文件加载为结构体。

#### 可行性 (Feasibility)
- **极高**: 底层库原生支持，仅需微调 `ConfigStore` 接口。
- **风险**: 低。

#### 修改范围 (Scope of Changes)

1.  **Core Library (`include/config/config.hpp`)**:
    - 修改 `get<T>(key)` 方法。
    - 当 `key` 为空字符串（`""`）时，尝试执行 `data_.get<T>()`，从而支持根节点转换。
    - 需要添加相应的异常处理逻辑（如类型不匹配时返回默认值或抛出异常）。

2.  **Examples (`examples/`)**:
    - 添加示例 `09_struct_serialization.cpp`。
    - 演示如何定义结构体、注册宏以及读取配置（包括数组和嵌套对象）。

#### 参考代码 (Reference)

```cpp
struct Item {
    std::string name;
    std::string value;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Item, name, value)

// Usage
auto items = store.get<std::vector<Item>>("");
```
