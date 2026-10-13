#pragma once

#include <exception>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace config
{
class Backend;
enum class SaveMode
{
    automatic,
    manual
};
enum class Format
{
    pretty,
    compact
};
enum class Encoding
{
    none,
    base64,
    hex,
    rot13,
    reverse,
    combined
};
enum class View
{
    effective,
    writable
};

struct Layer
{
    std::filesystem::path file;
    bool optional = false;
};

struct StoreOptions
{
    SaveMode save           = SaveMode::automatic;
    Format format           = Format::pretty;
    nlohmann::json defaults = nlohmann::json::object();
    std::vector<Layer> layers;
    std::string env_prefix;
    std::map<std::string, std::string> env_bindings;
    std::function<void(const nlohmann::json &)> validate;
    std::function<void(std::exception_ptr)> on_callback_error;
    std::shared_ptr<Backend> backend;
    // Explicit import of encoded 0.1.x files; normal user fields are not metadata.
    bool legacy_encoding = false;
};
} // namespace config
