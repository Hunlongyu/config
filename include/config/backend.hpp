#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace config
{
// Allow concurrent reads. Writes replace complete files or throw Error and retain the old content.
class Backend
{
  public:
    virtual ~Backend()                                                     = default;
    virtual std::optional<std::string> read(const std::filesystem::path &) = 0;
    virtual void write(const std::filesystem::path &, std::string_view)    = 0;
};
} // namespace config
