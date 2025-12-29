#pragma once

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <ranges>
#include <shared_mutex>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

/**
 * @brief Macro alias for nlohmann::json's non-intrusive struct serialization.
 * Use this to define JSON binding for a struct outside of the struct definition.
 */
#define CONFIG_STRUCT NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE

#if defined(_WIN32)
#include <shlobj.h>
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#elif defined(__linux__)
#include <limits.h>
#include <unistd.h>
#endif

namespace config
{

/**
 * @brief Enum defining path types for configuration files.
 */
enum class Path
{
    Absolute, ///< Absolute file path.
    Relative, ///< Relative path to the current working directory.
    AppData   ///< System-specific application data directory (e.g., %APPDATA% on Windows).
};

/**
 * @brief Enum defining JSON output formats.
 */
enum class JsonFormat
{
    Pretty, ///< Indented output (4 spaces) for readability.
    Compact ///< Minified output without whitespace.
};

/**
 * @brief Enum defining available obfuscation methods.
 */
enum class Obfuscate
{
    None,    ///< No obfuscation (plaintext).
    Base64,  ///< Base64 encoding.
    Hex,     ///< Hexadecimal string encoding.
    ROT13,   ///< ROT13 substitution cipher.
    Reverse, ///< String reversal.
    Combined ///< Combined strategy (Base64 + Reverse).
};

/**
 * @brief Enum defining when configuration changes are saved to disk.
 */
enum class SaveStrategy
{
    Auto,  ///< Automatically save to disk after every 'set' operation.
    Manual ///< Only save to disk when 'save()' is explicitly called.
};

/**
 * @brief Enum defining behavior when a key is missing during retrieval.
 */
enum class GetStrategy
{
    DefaultValue,  ///< Return a type-specific default value if key is missing.
    ThrowException ///< Throw a std::runtime_error if key is missing.
};

namespace detail
{

class ObfuscationEngine
{
    static constexpr char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    static constexpr bool is_base64(const unsigned char c)
    {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || (c == '+') || (c == '/');
    }

  public:
    static std::string base64_encode(std::string_view input)
    {
        std::string ret;
        ret.reserve(((input.length() + 2) / 3) * 4);
        int i = 0;
        unsigned char char_array_3[3];
        unsigned char char_array_4[4];
        auto it       = input.begin();
        size_t in_len = input.length();

        while (in_len--)
        {
            char_array_3[i++] = *(it++);
            if (i == 3)
            {
                char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
                char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
                char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
                char_array_4[3] = char_array_3[2] & 0x3f;

                for (i = 0; (i < 4); i++)
                    ret += base64_chars[char_array_4[i]];
                i = 0;
            }
        }

        if (i)
        {
            int j;
            for (j = i; j < 3; j++)
                char_array_3[j] = '\0';

            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (j = 0; (j < i + 1); j++)
                ret += base64_chars[char_array_4[j]];

            while ((i++ < 3))
                ret += '=';
        }

        return ret;
    }

    static std::string base64_decode(std::string_view input)
    {
        size_t in_len = input.size();
        int i         = 0;
        int in_       = 0;
        unsigned char char_array_4[4], char_array_3[3];
        std::string ret;

        while (in_len-- && (input[in_] != '=') && is_base64(input[in_]))
        {
            char_array_4[i++] = input[in_];
            in_++;
            if (i == 4)
            {
                for (i = 0; i < 4; i++)
                    char_array_4[i] = static_cast<unsigned char>(std::string_view(base64_chars).find(char_array_4[i]));

                char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
                char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
                char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

                for (i = 0; (i < 3); i++)
                    ret += char_array_3[i];
                i = 0;
            }
        }

        if (i)
        {
            int j;
            for (j = i; j < 4; j++)
                char_array_4[j] = 0;

            for (j = 0; j < 4; j++)
                char_array_4[j] = static_cast<unsigned char>(std::string_view(base64_chars).find(char_array_4[j]));

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (j = 0; (j < i - 1); j++)
                ret += char_array_3[j];
        }

        return ret;
    }

    static std::string hex_encode(std::string_view input)
    {
        std::string result;
        result.reserve(input.length() * 2);
        for (const unsigned char c : input)
        {
            std::format_to(std::back_inserter(result), "{:02x}", c);
        }
        return result;
    }

    static std::string hex_decode(std::string_view input)
    {
        std::string result;
        if (input.length() % 2 == 0)
        {
            for (size_t i = 0; i < input.length(); i += 2)
            {
                std::string byteString(input.substr(i, 2));
                const char byte = static_cast<char>(strtol(byteString.c_str(), nullptr, 16));
                result += byte;
            }
        }
        return result;
    }

    static std::string rot13(std::string_view input)
    {
        std::string result;
        result.reserve(input.size());
        auto transform_char = [](char c) -> char {
            if (c >= 'a' && c <= 'z')
                return (c - 'a' + 13) % 26 + 'a';
            if (c >= 'A' && c <= 'Z')
                return (c - 'A' + 13) % 26 + 'A';
            return c;
        };
        std::ranges::transform(input, std::back_inserter(result), transform_char);
        return result;
    }

    static std::string reverse(std::string_view input)
    {
        std::string result(input);
        std::ranges::reverse(result);
        return result;
    }

    static std::string encrypt(std::string_view input, const Obfuscate obf)
    {
        std::string result;
        switch (obf)
        {
        case Obfuscate::Base64:
            result = base64_encode(input);
            break;
        case Obfuscate::Hex:
            result = hex_encode(input);
            break;
        case Obfuscate::ROT13:
            result = rot13(input);
            break;
        case Obfuscate::Reverse:
            result = reverse(input);
            break;
        case Obfuscate::Combined:
            result = reverse(base64_encode(input));
            break;
        case Obfuscate::None:
        default:
            result = std::string(input);
            break;
        }
        return result;
    }

    static std::string decrypt(std::string_view input, const Obfuscate obf)
    {
        std::string result;
        switch (obf)
        {
        case Obfuscate::Base64:
            result = base64_decode(input);
            break;
        case Obfuscate::Hex:
            result = hex_decode(input);
            break;
        case Obfuscate::ROT13:
            result = rot13(input); // ROT13 is symmetric
            break;
        case Obfuscate::Reverse:
            result = reverse(input); // Reverse is symmetric
            break;
        case Obfuscate::Combined:
            result = base64_decode(reverse(input));
            break;
        case Obfuscate::None:
        default:
            result = std::string(input);
            break;
        }
        return result;
    }
};

class PathResolver
{
  public:
    static std::string get_program_name()
    {
        std::string name = "config_app";
#if defined(CONFIG_TEST_FORCE_FALLBACK_PROGNAME)
        // Fallback testing mode: skip platform-specific logic
#elif defined(_WIN32)
        char module_path[MAX_PATH]{};
        const DWORD r = GetModuleFileNameA(nullptr, module_path, MAX_PATH);
        if (r > 0 && r < MAX_PATH)
        {
            const std::filesystem::path p(module_path);
            name = p.stem().string();
        }
#elif defined(__APPLE__)
        char buf[4096];
        uint32_t size = sizeof(buf);
        if (_NSGetExecutablePath(buf, &size) == 0)
        {
            std::filesystem::path p(buf);
            name = p.stem().string();
        }
#elif defined(__linux__)
        char buf[PATH_MAX];
        ssize_t len = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (len > 0)
        {
            buf[len] = '\0';
            std::filesystem::path p(buf);
            name = p.stem().string();
        }
#endif
        return name;
    }

    static std::filesystem::path get_appdata_path()
    {
#if defined(CONFIG_TEST_FORCE_FALLBACK_APPDATA)
        return std::filesystem::current_path();
#else
        std::filesystem::path app_path;
#if defined(_WIN32)
        WCHAR buff[MAX_PATH]{};
        bool success = false;
        success      = SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buff));
        if (success)
        {
            app_path = std::filesystem::path(std::wstring(buff)) / get_program_name();
        }
        else
        {
            app_path = std::filesystem::current_path();
        }
#elif defined(__APPLE__)
        const char *home = std::getenv("HOME");
        if (home)
            app_path = std::filesystem::path(home) / "Library" / "Application Support" / get_program_name();
        else
            app_path = std::filesystem::current_path();
#else
        const char *xdg  = std::getenv("XDG_CONFIG_HOME");
        const char *home = std::getenv("HOME");
        if (xdg)
            app_path = std::filesystem::path(xdg) / get_program_name();
        else if (home)
            app_path = std::filesystem::path(home) / ".config" / get_program_name();
        else
            app_path = std::filesystem::current_path();
#endif
        return app_path;
#endif
    }

    static std::string resolve(const std::string &path, const Path type)
    {
        const std::filesystem::path p(path);
        std::string result;
        std::filesystem::path app_data;

        switch (type)
        {
        case Path::AppData:
            app_data = get_appdata_path();
            if (!std::filesystem::exists(app_data))
            {
                std::filesystem::create_directories(app_data);
            }
            result = (app_data / p).string();
            break;
        case Path::Absolute:
        case Path::Relative:
        default:
            result = std::filesystem::absolute(p).string();
            break;
        }
        return result;
    }
};

} // namespace detail

/**
 * @brief Concept to ensure a type can be retrieved from JSON.
 */
template <typename T>
concept JsonReadable = requires(nlohmann::json j) { j.get<T>(); };

/**
 * @brief Concept to ensure a type can be written to JSON.
 */
template <typename T>
concept JsonWritable = requires(nlohmann::json j, T v) { j = v; };

/**
 * @brief Thread-safe configuration store managing JSON data persistence and retrieval.
 *
 * Provides thread-safe access to configuration data stored in JSON format.
 * Supports strategies for saving behavior, path resolution, and error handling.
 */
class ConfigStore
{
  public:
    using json             = nlohmann::json;
    using ListenerId       = size_t;
    using ListenerCallback = std::function<void(const json &)>;

  private:
    std::string file_path_;
    Path path_type_;
    SaveStrategy save_strategy_;
    GetStrategy get_strategy_;
    JsonFormat json_format_ = JsonFormat::Pretty;

    mutable std::shared_mutex mutex_;
    json data_;
    std::unordered_map<std::string, Obfuscate> obfuscation_map_;

    struct Listener
    {
        ListenerId id;
        std::string key;
        ListenerCallback callback;
    };

    std::vector<Listener> listeners_;
    std::atomic<size_t> next_listener_id_{1};

    static constexpr const char *META_OBFUSCATION_KEY = "__obfuscate_meta__";

    void load()
    {
        if (std::filesystem::exists(file_path_))
        {
            try
            {
                std::ifstream file(file_path_);
                json loaded_data;
                file >> loaded_data;

                if (loaded_data.contains(META_OBFUSCATION_KEY))
                {
                    auto meta = loaded_data[META_OBFUSCATION_KEY];
                    for (auto &[key, val] : meta.items())
                    {
                        obfuscation_map_[key] = static_cast<Obfuscate>(val.get<int>());
                    }
                    loaded_data.erase(META_OBFUSCATION_KEY);
                }

                for (const auto &[key, type] : obfuscation_map_)
                {
                    if (type == Obfuscate::None)
                        continue;

                    std::string ptr_str = (key.front() == '/') ? key : "/" + key;
                    try
                    {
                        nlohmann::json::json_pointer ptr(ptr_str);
                        if (loaded_data.contains(ptr))
                        {
                            auto &val = loaded_data[ptr];
                            if (val.is_string())
                            {
                                val = detail::ObfuscationEngine::decrypt(val.get<std::string>(), type);
                            }
                        }
                    }
                    catch (...)
                    {
                        // std::cout << "Caught exception for key: " << key << std::endl;
                        if (loaded_data.contains(key))
                        {
                            auto &val = loaded_data[key];
                            if (val.is_string())
                            {
                                val = detail::ObfuscationEngine::decrypt(val.get<std::string>(), type);
                            }
                        }
                    }
                }

                data_ = loaded_data;
            }
            catch (...)
            {
                data_ = json::object();
            }
        }
        else
        {
            data_ = json::object();
        }
    }

    void notify(std::string_view key, [[maybe_unused]] const json &val) const
    {
        for (const auto &l : listeners_)
        {
            if (key == l.key || key.find(l.key + "/") == 0)
            {
                try
                {
                    std::string ptr_str = (l.key.front() == '/') ? l.key : "/" + l.key;
                    auto v              = get_value_at(ptr_str);
                    l.callback(v);
                }
                catch (...)
                {
                }
            }
        }
    }

    json get_value_at(std::string_view key_or_ptr) const
    {
        std::shared_lock lock(mutex_);
        // key_or_ptr is guaranteed to be non-empty by caller (notify) logic.
        const std::string ptr_str =
            (key_or_ptr.front() == '/') ? std::string(key_or_ptr) : "/" + std::string(key_or_ptr);
        json result;
        try
        {
#if defined(CONFIG_TEST_FORCE_GET_VALUE_EXCEPTION)
            throw std::runtime_error("Forced exception");
#endif
            result = data_.at(nlohmann::json::json_pointer(ptr_str));
        }
        catch (...)
        {
            result = json();
        }
        return result;
    }

  public:
    /**
     * @brief Constructs a new ConfigStore instance.
     *
     * @param path File path for the configuration file.
     * @param type Strategy for resolving the file path.
     * @param save_strategy Strategy for saving changes (Auto or Manual).
     * @param get_strategy Strategy for handling missing keys (DefaultValue or ThrowException).
     */
    ConfigStore(const std::string &path, const Path type = Path::Relative,
                const SaveStrategy save_strategy = SaveStrategy::Auto,
                const GetStrategy get_strategy   = GetStrategy::DefaultValue)
        : path_type_(type), save_strategy_(save_strategy), get_strategy_(get_strategy)
    {
        file_path_ = detail::PathResolver::resolve(path, type);
        load();
    }

    ConfigStore(const ConfigStore &)            = delete;
    ConfigStore &operator=(const ConfigStore &) = delete;
    ConfigStore(ConfigStore &&)                 = delete;
    ConfigStore &operator=(ConfigStore &&)      = delete;

    /**
     * @brief Gets the absolute path to the configuration file.
     * @return Absolute file path string.
     */
    std::string get_store_path() const
    {
        return file_path_;
    }

    /**
     * @brief Sets the strategy for saving configuration changes.
     * @param strategy The new SaveStrategy (Auto or Manual).
     */
    void set_save_strategy(const SaveStrategy strategy)
    {
        std::unique_lock lock(mutex_);
        save_strategy_ = strategy;
    }

    /**
     * @brief Gets the current save strategy.
     * @return Current SaveStrategy.
     */
    SaveStrategy get_save_strategy() const
    {
        std::shared_lock lock(mutex_);
        return save_strategy_;
    }

    /**
     * @brief Sets the strategy for handling missing keys.
     * @param strategy The new GetStrategy (DefaultValue or ThrowException).
     */
    void set_get_strategy(const GetStrategy strategy)
    {
        std::unique_lock lock(mutex_);
        get_strategy_ = strategy;
    }

    /**
     * @brief Gets the current retrieval strategy.
     * @return Current GetStrategy.
     */
    GetStrategy get_get_strategy() const
    {
        std::shared_lock lock(mutex_);
        return get_strategy_;
    }

    /**
     * @brief Sets the JSON output format.
     * @param format The new JsonFormat (Pretty or Compact).
     */
    void set_format(const JsonFormat format)
    {
        std::unique_lock lock(mutex_);
        json_format_ = format;
    }

    /**
     * @brief Gets the current JSON output format.
     * @return Current JsonFormat.
     */
    JsonFormat get_format() const
    {
        std::shared_lock lock(mutex_);
        return json_format_;
    }

    /**
     * @brief Retrieves a value from the configuration with a default fallback.
     *
     * @tparam T Type of the value to retrieve.
     * @param key The configuration key or JSON Pointer path (e.g., "section/key").
     * @param default_value The value to return if the key is not found.
     * @return The retrieved value or default_value.
     */
    template <typename T>
        requires JsonReadable<T>
    T get(std::string_view key, const T &default_value) const
    {
        std::shared_lock lock(mutex_);
        if (key.empty())
        {
            return default_value;
        }

        const std::string ptr_str = (key.front() == '/') ? std::string(key) : "/" + std::string(key);
        const nlohmann::json::json_pointer ptr(ptr_str);
        if (data_.contains(ptr))
        {
            try
            {
                return data_.at(ptr).get<T>();
            }
            catch (...)
            {
                // Fallthrough to return default_value
            }
        }
        return default_value;
    }

    /**
     * @brief Retrieves a value from the configuration.
     *
     * Behavior depends on the current GetStrategy if the key is missing:
     * - DefaultValue: Returns T{}
     * - ThrowException: Throws std::runtime_error
     *
     * @tparam T Type of the value to retrieve.
     * @param key The configuration key or JSON Pointer path.
     * @param location
     * @return The retrieved value.
     * @throws std::runtime_error If key is missing and strategy is ThrowException.
     */
    template <typename T>
        requires JsonReadable<T>
    T get(std::string_view key, const std::source_location location = std::source_location::current()) const
    {
        std::shared_lock lock(mutex_);
        if (key.empty())
        {
            try
            {
                return data_.get<T>();
            }
            catch (...)
            {
                if (get_strategy_ == GetStrategy::ThrowException)
                {
                    throw std::runtime_error(std::format("Root conversion failed ({}:{}:{})", location.file_name(),
                                                         location.line(), location.function_name()));
                }
                return T{};
            }
        }

        const std::string ptr_str = (key.front() == '/') ? std::string(key) : "/" + std::string(key);
        const nlohmann::json::json_pointer ptr(ptr_str);
        if (data_.contains(ptr))
        {
            try
            {
                return data_.at(ptr).get<T>();
            }
            catch (...)
            {
                // Type mismatch: treat as missing key / default value case
            }
        }

        if (get_strategy_ == GetStrategy::ThrowException)
        {
            throw std::runtime_error(std::format("Key not found: {} ({}:{}:{})", key, location.file_name(),
                                                 location.line(), location.function_name()));
        }
        return T{};
    }

    /**
     * @brief Sets a value in the configuration.
     *
     * @tparam T Type of the value to set.
     * @param key The configuration key or JSON Pointer path.
     * @param value The value to store.
     * @param obf Obfuscation method to apply (optional).
     * @param location
     * @return true if the operation succeeded (including auto-save if enabled), false if auto-save failed.
     * @throws std::runtime_error If setting the value fails in memory (e.g., path conflict).
     */
    template <typename T>
        requires JsonWritable<T>
    bool set(std::string_view key, const T &value, const Obfuscate obf = Obfuscate::None,
             const std::source_location location = std::source_location::current())
    {
        if (key.empty())
        {
            return false;
        }

        std::string error_msg;
        {
            std::unique_lock lock(mutex_);
            const std::string ptr_str = (key.front() == '/') ? std::string(key) : "/" + std::string(key);
            try
            {
                data_[nlohmann::json::json_pointer(ptr_str)] = value;

                if (obf != Obfuscate::None)
                {
                    obfuscation_map_[std::string(key)] = obf;
                }
                else
                {
                    obfuscation_map_.erase(std::string(key));
                }
            }
            catch (const std::exception &e)
            {
                error_msg = std::format("Config set failed for key '{}': {} ({}:{}:{})", key, e.what(),
                                        location.file_name(), location.line(), location.function_name());
            }
        }

        if (!error_msg.empty())
        {
            throw std::runtime_error(error_msg);
        }

        notify(key, json(value));

        bool result = true;
        if (save_strategy_ == SaveStrategy::Auto)
        {
            result = save();
        }
        return result;
    }

    /**
     * @brief Removes a key and its value from the configuration.
     * @param key The configuration key or JSON Pointer path to remove.
     * @return true if the operation succeeded (including auto-save if enabled), false if auto-save failed.
     */
    bool remove(std::string_view key)
    {
        {
            std::unique_lock lock(mutex_);
            std::string ptr_str;
            if (key.empty())
            {
                ptr_str = "/";
            }
            else
            {
                ptr_str = (key.front() == '/') ? std::string(key) : "/" + std::string(key);
            }

            try
            {
                const nlohmann::json::json_pointer ptr(ptr_str);

                const auto parent_ptr = ptr.parent_pointer();
                if (data_.contains(parent_ptr))
                {
                    auto &parent = data_[parent_ptr];
                    parent.erase(ptr.back());
                }
                obfuscation_map_.erase(std::string(key));
            }
            catch (...)
            {
            }
        }
        bool result = true;
        if (save_strategy_ == SaveStrategy::Auto)
        {
            result = save();
        }
        return result;
    }

    /**
     * @brief Checks if a key exists in the configuration.
     * @param key The configuration key or JSON Pointer path.
     * @return true if the key exists, false otherwise.
     */
    bool contains(std::string_view key) const
    {
        bool result = false;
        std::shared_lock lock(mutex_);
        if (!key.empty())
        {
            const std::string ptr_str = (key.front() == '/') ? std::string(key) : "/" + std::string(key);
            result                    = data_.contains(nlohmann::json::json_pointer(ptr_str));
        }
        return result;
    }

    /**
     * @brief Saves the current configuration to disk using the current format.
     * @return true if saved successfully, false otherwise.
     */
    bool save() const
    {
        return save(json_format_);
    }

    /**
     * @brief Saves the current configuration to disk using a specific format.
     * @param format The output format (Pretty or Compact).
     * @return true if saved successfully, false otherwise.
     */
    bool save(JsonFormat format) const
    {
        bool result = false;
        json save_data;
        std::unordered_map<std::string, Obfuscate> obf_map_copy;

        {
            std::shared_lock lock(mutex_);
            save_data    = data_;
            obf_map_copy = obfuscation_map_;
        }

        try
        {
            std::filesystem::path p(file_path_);
            if (p.has_parent_path())
            {
                std::filesystem::create_directories(p.parent_path());
            }

            if (!obf_map_copy.empty())
            {
                for (const auto &[key, type] : obf_map_copy)
                {
                    if (type == Obfuscate::None)
                        continue;

                    std::string ptr_str = (key.front() == '/') ? key : "/" + key;
                    try
                    {
                        nlohmann::json::json_pointer ptr(ptr_str);
                        if (save_data.contains(ptr))
                        {
                            auto &val = save_data[ptr];
                            if (val.is_string())
                            {
                                val = detail::ObfuscationEngine::encrypt(val.get<std::string>(), type);
                            }
                        }
                    }
                    catch (...)
                    {
                    }
                }

                json meta;
                for (const auto &[key, val] : obf_map_copy)
                {
                    meta[key] = static_cast<int>(val);
                }
                save_data[META_OBFUSCATION_KEY] = meta;
            }

            std::ofstream file(file_path_);
            if (file.is_open())
            {
                if (format == JsonFormat::Pretty)
                {
                    file << save_data.dump(4);
                }
                else
                {
                    file << save_data.dump();
                }
                result = file.good();
            }
        }
        catch (...)
        {
            result = false;
        }
        return result;
    }

    /**
     * @brief Reloads configuration from disk, discarding current memory state.
     */
    void reload()
    {
        std::unique_lock lock(mutex_);
        load();
    }

    /**
     * @brief Clears all configuration data and obfuscation rules.
     * If SaveStrategy is Auto, this change is immediately persisted to disk.
     *
     * @return true if the operation succeeded (including auto-save if enabled), false if auto-save failed.
     */
    bool clear()
    {
        {
            std::unique_lock lock(mutex_);
            data_.clear();
            obfuscation_map_.clear();
        }
        bool result = true;
        if (save_strategy_ == SaveStrategy::Auto)
        {
            result = save();
        }
        return result;
    }

    /**
     * @brief Connects a listener callback to a specific key.
     *
     * The callback will be invoked whenever the value of the key (or its children) changes.
     *
     * @param key The key to listen to.
     * @param callback Function to call on change.
     * @return ID of the listener (used for disconnecting).
     */
    size_t connect(const std::string &key, const std::function<void(const nlohmann::json &)> &callback)
    {
        std::unique_lock lock(mutex_);
        const size_t id = next_listener_id_++;
        listeners_.push_back({id, key, callback});
        return id;
    }

    /**
     * @brief Disconnects a listener by its ID.
     * @param connection_id The ID returned by connect().
     */
    void disconnect(size_t connection_id)
    {
        std::unique_lock lock(mutex_);
        listeners_.erase(
            std::ranges::remove_if(listeners_, [connection_id](const Listener &l) { return l.id == connection_id; })
                .begin(),
            listeners_.end());
    }
};

namespace detail
{
struct StringHash
{
    using is_transparent = void;
    size_t operator()(std::string_view sv) const
    {
        return std::hash<std::string_view>{}(sv);
    }
    size_t operator()(const std::string &s) const
    {
        return std::hash<std::string>{}(s);
    }
    size_t operator()(const char *s) const
    {
        return std::hash<std::string_view>{}(s);
    }
};
} // namespace detail

namespace registry
{
inline std::unordered_map<std::string, std::shared_ptr<ConfigStore>, detail::StringHash, std::equal_to<>> &get_stores()
{
    static std::unordered_map<std::string, std::shared_ptr<ConfigStore>, detail::StringHash, std::equal_to<>> stores;
    return stores;
}
inline std::mutex &get_mutex()
{
    static std::mutex mtx;
    return mtx;
}
} // namespace registry

/**
 * @brief Retrieves or creates a ConfigStore instance.
 *
 * This is the primary entry point for managing multiple configuration files.
 * Instances are cached by path.
 *
 * @param path File path for the configuration.
 * @param type Strategy for resolving the file path.
 * @param save_strategy Strategy for saving changes.
 * @param get_strategy Strategy for handling missing keys.
 * @return Reference to the ConfigStore instance.
 */
inline ConfigStore &get_store(std::string_view path, Path type = Path::Relative,
                              SaveStrategy save_strategy = SaveStrategy::Auto,
                              GetStrategy get_strategy   = GetStrategy::DefaultValue)
{
    std::lock_guard<std::mutex> lock(registry::get_mutex());
    auto &stores = registry::get_stores();
    auto it      = stores.find(path);
    if (it == stores.end())
    {
        std::string path_str(path);
        auto [new_it, inserted] =
            stores.emplace(path_str, std::make_shared<ConfigStore>(path_str, type, save_strategy, get_strategy));
        it = new_it;
    }
    return *it->second;
}

inline ConfigStore &get_default_store()
{
    return get_store("config.json");
}

/**
 * @brief Global convenience function: Sets save strategy for the default store.
 */
inline void set_save_strategy(const SaveStrategy strategy)
{
    get_default_store().set_save_strategy(strategy);
}
/**
 * @brief Global convenience function: Gets save strategy of the default store.
 */
inline SaveStrategy get_save_strategy()
{
    return get_default_store().get_save_strategy();
}
/**
 * @brief Global convenience function: Sets get strategy for the default store.
 */
inline void set_get_strategy(const GetStrategy strategy)
{
    get_default_store().set_get_strategy(strategy);
}
/**
 * @brief Global convenience function: Gets get strategy of the default store.
 */
inline GetStrategy get_get_strategy()
{
    return get_default_store().get_get_strategy();
}

/**
 * @brief Global convenience function: Gets a value from the default store with a default fallback.
 */
template <typename T> T get(std::string_view key, const T &default_value)
{
    return get_default_store().get<T>(key, default_value);
}
/**
 * @brief Global convenience function: Gets a value from the default store.
 */
template <typename T> T get(std::string_view key)
{
    return get_default_store().get<T>(key);
}
/**
 * @brief Global convenience function: Sets a value in the default store.
 */
template <typename T> bool set(std::string_view key, const T &value, Obfuscate obf = Obfuscate::None)
{
    return get_default_store().set(key, value, obf);
}
/**
 * @brief Global convenience function: Removes a key from the default store.
 */
inline bool remove(std::string_view key)
{
    return get_default_store().remove(key);
}
/**
 * @brief Global convenience function: Checks if a key exists in the default store.
 */
inline bool contains(std::string_view key)
{
    return get_default_store().contains(key);
}

/**
 * @brief Global convenience function: Saves the default store to disk.
 */
inline bool save()
{
    return get_default_store().save();
}
/**
 * @brief Global convenience function: Saves the default store to disk with format.
 */
inline bool save(const JsonFormat format)
{
    return get_default_store().save(format);
}
/**
 * @brief Global convenience function: Reloads the default store from disk.
 */
inline void reload()
{
    get_default_store().reload();
}
/**
 * @brief Global convenience function: Clears the default store.
 */
inline bool clear()
{
    return get_default_store().clear();
}

/**
 * @brief Global convenience function: Sets format for the default store.
 */
inline void set_format(const JsonFormat format)
{
    get_default_store().set_format(format);
}
/**
 * @brief Global convenience function: Gets format of the default store.
 */
inline JsonFormat get_format()
{
    return get_default_store().get_format();
}

/**
 * @brief Global convenience function: Gets the file path of the default store.
 */
inline std::string get_store_path()
{
    return get_default_store().get_store_path();
}

} // namespace config
