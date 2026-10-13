# API 参考 — 0.2

[English](API.md) · [首页](../README.zh-CN.md) · [迁移](MIGRATION.zh-CN.md)

主入口为 `<config/config.hpp>`。监视器和路径工具分别包含 `<config/watcher.hpp>`、`<config/paths.hpp>`。

## Store

`Store(std::filesystem::path file, StoreOptions options = {})` 打开一个可写文件。相对路径在构造时按当前工作目录解析。选项复制后不可变；Store 不可复制、不可移动，需要共享时显式管理所有权。

不存在的可写文件从空对象开始，构造不创建目录。已有文件无法读取、格式损坏或根不是对象时抛错。同一文件的重复活动所有者抛出 `in_use`，不永久缓存旧实例。

| 操作 | 契约 |
|---|---|
| `get<T>(pointer)` | 读取有效值，缺失报 missing，转换失败报 type。 |
| `get_or<T>(pointer, fallback)` | 仅缺失时回退，类型错误和非法路径不回退。 |
| `contains(pointer)` | 检查与 get、snapshot 相同的有效视图。 |
| `snapshot(View::effective)` | 返回包含全部配置层的根对象副本。 |
| `snapshot(View::writable)` | 仅返回可写层副本。 |
| `set(pointer, json, Encoding::none)` | 修改可写值；自定义类型使用 nlohmann 序列化。编码仅适用于字符串。 |
| `erase(pointer)` | 删除可写值，可能重新显示低优先级值；不存在则不删除。 |
| `update(fn)` | 修改可写根对象副本，统一校验并一次提交，根必须保持对象。 |
| `merge(json)` | 对象递归合并，数组和标量整体替换。 |
| `merge_file(path)` | 经过统一解码后合并；文件缺失报错。 |
| `save()` | 序列化当前可写层；失败抛错并保留旧磁盘文件。 |
| `reload()` | 重建全部来源并刷新环境；成功时显式丢弃尚未保存的可写修改。 |
| `subscribe(callback)` | 返回只能移动的 nodiscard Subscription；回调参数为 const Change&。 |
| `path()` | 解析后的绝对文件路径。 |
| `revision()` | 单调递增的提交版本；save 也可能递增，但不产生有效值事件。 |
| `dirty()` | 可写层存在未持久化修改，save/reload 成功后清除。 |

键是以 `/` 开头的非空 JSON Pointer。`~1` 转义斜杠，`~0` 转义波浪线；空属性段有效。禁止空指针，根操作使用 snapshot/update。数组寻址遵循 nlohmann JSON Pointer 语义。

批量回调在内部锁外、针对候选副本执行一次；回调自身异常继续传播，JSON 路径操作错误转换成 config::Error。并发提交可能导致 conflict，库不会隐式重复用户函数。

## StoreOptions

| 字段 | 默认值及含义 |
|---|---|
| save | SaveMode::automatic，也可选 manual。 |
| format | Format::pretty，也可选 compact。 |
| defaults | 空 JSON 对象，最低优先级，不保存。 |
| layers | 有序 Layer{file, optional=false}；后面的层优先。 |
| env_prefix | 空表示关闭；后缀 ASCII 字母转小写，下划线转路径分隔符。 |
| env_bindings | JSON Pointer 到环境变量名的映射；显式绑定优先于前缀映射。 |
| validate | 可选 void(const json&)，构造、每次修改和重载均校验候选有效视图。 |
| on_callback_error | 可选 void(std::exception_ptr)，接收订阅回调异常。 |
| backend | 可选共享 Backend，默认原子文件后端。 |
| legacy_encoding | 默认 false，仅导入 0.1 编码文件时开启。 |

可选层只跳过缺失文件，损坏或不可读的层仍报错。环境值可解析为 JSON 时使用解析结果，否则作为字符串；仅在构造和 reload 时采样。修改进程环境需要调用方同步，库无法锁住外部环境写入者。

读取顺序为默认值 → 文件层 → 可写层 → 环境。保存不会把低优先级层或环境值复制进可写层。环境值可以遮住一次成功的 set，事件只描述有效视图变化。

校验器在锁外执行，可读取已提交状态。建议保持纯函数；校验中修改或保存同一个 Store 会使当前事务发生冲突。校验器抛出的异常转换为 validation。

## 错误与保存

Error 继承 std::runtime_error；code() 和 file() 提供类型及相关文件。错误码包括 missing、type、path、read、parse、write、validation、conflict、in_use、format。

自动修改顺序：准备 → 校验 → 持久化 → 提交 → 通知。读取、解析、校验、序列化、冲突或写入失败均不能发布候选状态和事件。手动模式先提交内存并标记 dirty；随后保存失败会保留这些明确未保存的修改。内存分配失败及用户回调异常遵循标准 C++ 异常语义。

0.2 输出文件格式：
```json
{"$config":"config/v1","values":{"port":8080},"encoding":{}}
```
encoding 使用规范 JSON Pointer 映射到 0–5，依次表示 none/base64/hex/rot13/reverse/combined。业务字段位于 values 内，包含 __obfuscate_meta__ 和 $config 在内的字段均可完整往返。支持普通对象导入，但顶层以 config/ 开头的 $config 字段保留给格式封装；未知版本或非法编码规则报错。

编码可逆，不保护秘密。替换子树会清理其编码规则；删除数组元素会移动后续规则。任意 update/merge 会清理值发生变化的编码规则，需要新编码时使用 set。

## Subscription 与 Change

Change{revision, before, after} 表示一次已提交的有效视图变更。有效视图相等不发事件，单独 save 不发事件；逐键差异可使用 json::diff(before, after)。

必须保存订阅句柄。析构或 disconnect() 阻止后续回调开始，但不等待已经执行的回调。断开或 Store 销毁后 connected() 为 false。移动赋值会断开目标原先的订阅。

事件按提交顺序在状态锁、写锁之外派发。回调可读取、修改、注册或取消订阅；重入事件排在当前批次之后。另一线程正在派发时，修改函数可以先于自身事件完成返回。回调运行于派发线程，UI 切线程由调用方处理。新订阅不会收到已排队事件。回调异常被隔离并交给 on_callback_error，其自身异常忽略。

## Watcher 与路径

Watcher(Store&, interval=1s, on_error={}) 轮询可写文件和文件层内容，相同时间戳也能识别内容变化。候选提交失败不会确认版本，下次继续重试；成功重载后清除 last_error()。错误回调在锁外执行。仅环境变量变化需要显式 reload。存在未保存手动修改时，监视重载报 conflict，避免丢失修改。

stop() 可以中断轮询等待，无法中断已经运行的用户回调或文件操作。重复调用安全；并发停止时，一个调用方负责 join，其余可能提前返回。自身回调内析构时先取消再 detach，线程状态独立于 Watcher 对象。Watcher 可以比 Store 活得更久，也不会永久保留 Store。

user_config_directory(application) 返回路径而不创建目录。application 必须是非空单级目录名。Windows 使用 LocalAppData，macOS 使用 ~/Library/Application Support，Linux 使用绝对 XDG_CONFIG_HOME 或 ~/.config。无法确定目录时报错，不静默回退当前目录。

## Backend

read(path) 返回可选文件字节，仅缺失返回 nullopt；write(path, bytes) 完整原子替换或抛 Error 并保留旧文件。实现必须允许并发读取、转换存储错误，不得在 write 内重入 Store。此扩展边界也用于确定性故障和并发测试。
