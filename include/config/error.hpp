#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>

namespace config
{
enum class ErrorCode
{
    missing,
    type,
    path,
    read,
    parse,
    write,
    validation,
    conflict,
    in_use,
    format
};

class Error : public std::runtime_error
{
    ErrorCode code_;
    std::filesystem::path file_;
    std::string key_;

  public:
    Error(ErrorCode code, std::string message, std::filesystem::path file = {}, std::string key = {})
        : std::runtime_error(std::move(message)), code_(code), file_(std::move(file)), key_(std::move(key))
    {
    }
    [[nodiscard]] ErrorCode code() const noexcept
    {
        return code_;
    }
    [[nodiscard]] const std::filesystem::path &file() const noexcept
    {
        return file_;
    }
    [[nodiscard]] const std::string &key() const noexcept
    {
        return key_;
    }
};
} // namespace config
