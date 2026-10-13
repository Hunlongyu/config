# 从 0.1 迁移到 0.2

[English](MIGRATION.md) · [API](API.zh-CN.md)

本次主动移除重复和隐式 API，不提供保留隐藏全局状态的兼容包装。

| 0.1 | 0.2 |
|---|---|
| ConfigStore 及位置策略参数 | Store(file, StoreOptions) |
| 全局 get/set/default store/get_store/release_store | 显式持有并向调用方共享 Store |
| Path::Relative / Absolute | std::filesystem::path |
| Path::AppData | user_config_directory(application) / filename |
| SaveStrategy::Auto / Manual | 不可变选项中的 SaveMode::automatic / manual |
| MissingKeyPolicy / set_missing_key_policy | get 严格读取，get_or 显式回退 |
| get(key, fallback) | get_or<T>(pointer, fallback) |
| 普通键、空根键 | /key；根使用 snapshot/update |
| remove / clear / set_root | erase / update(fn) |
| get_root / sub / dump | snapshot / get<json> / snapshot().dump() |
| keys / children / all_keys / get_all | 使用 JSON 快照的 items 或 JSON Pointer 遍历 |
| get_or_set | update(fn)，明确可写层初始化条件 |
| set_default / clear_defaults | StoreOptions.defaults，不再动态修改来源策略 |
| bind_env / load_layered | StoreOptions.env_bindings / layers，reload 刷新来源 |
| connect / on_change / on_any_change / 数字 ID 退订 | subscribe(Change 回调)、Subscription::disconnect |
| start_watch / stop_watch | 独立作用域 Watcher |
| get_store_path / path | path() |
| CONFIG_STRUCT 宏 | nlohmann 标准 NLOHMANN_DEFINE_TYPE_* 宏 |
| JsonFormat::Pretty / Compact | Format::pretty / compact |
| Encoding::Base64 等 | Encoding::base64、hex、rot13、reverse、combined、none |

迁移时先阅读 [API 契约](API.zh-CN.md)。save 现在返回 void 并抛 Error；自动保存失败不修改内存、不发通知。类型错误不再伪装成缺失。校验器覆盖全部候选状态，merge/reload 也参与有效视图事件。

默认值、文件层和环境值不再作为用户修改写回。显式 reload 丢弃未保存修改，Watcher 拒绝丢弃。并发事务可能报 conflict，用户修改函数不会隐式重试。

## 现有文件

普通 JSON 对象可以直接导入，保存后变为包含 values 和 encoding 的 config/v1 封装。

0.1 编码文件迁移步骤：
1. 备份原文件。
2. 仅在此次导入时设置 StoreOptions.legacy_encoding=true。
3. 检查解码结果，调用 save()。
4. 恢复普通选项重新打开，后续文件使用有版本格式。

只对真实旧编码文件开启该选项，它明确将 __obfuscate_meta__ 解释为元数据。默认导入则将该名字保留为业务字段。可逆编码不保护秘密。

可选文件层现在只跳过缺失文件，损坏的可选文件仍报错。未知编码规则、非法根结构、不支持的格式版本均明确失败。原来依赖静默空配置回退的应用需要调整错误处理。

## 测试整理

原 combined/persistence 重复测试已合并为按行为组织的测试组。旧回归分别迁入 subscriptions、concurrency、persistence、sources。注册表引用测试替换为单所有者和订阅生命周期测试；可变策略竞争测试替换为不可变选项与事务测试。旧枚举别名、全局函数、隐式根处理、程序名回退已明确移除，不再保留多套契约。
