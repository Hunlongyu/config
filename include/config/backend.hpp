#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace config
{
// 实现必须允许并发读取；write 成功时完整替换，失败时保留旧内容并抛出 Error。
class Backend
{
  public:
    virtual ~Backend()                                                     = default;
    virtual std::optional<std::string> read(const std::filesystem::path &) = 0;
    virtual void write(const std::filesystem::path &, std::string_view)    = 0;
};
} // namespace config
