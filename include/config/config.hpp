#pragma once

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

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

    static bool is_base64(const unsigned char c)
    {
        return (isalnum(c) || (c == '+') || (c == '/'));
    }

  public:
    static std::string base64_encode(const std::string &input)
    {
        std::string ret;
        int i = 0;
        unsigned char char_array_3[3];
        unsigned char char_array_4[4];
        const char *bytes_to_encode = input.c_str();
        size_t in_len               = input.length();

        while (in_len--)
        {
            char_array_3[i++] = *(bytes_to_encode++);
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

    static std::string base64_decode(const std::string &input)
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
                    char_array_4[i] = static_cast<unsigned char>(std::string(base64_chars).find(char_array_4[i]));

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
                char_array_4[j] = static_cast<unsigned char>(std::string(base64_chars).find(char_array_4[j]));

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (j = 0; (j < i - 1); j++)
                ret += char_array_3[j];
        }

        return ret;
    }

    static std::string hex_encode(const std::string &input)
    {
        std::stringstream ss;
        for (const unsigned char c : input)
        {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
        }
        return ss.str();
    }

    static std::string hex_decode(const std::string &input)
    {
        std::string result;
        if (input.length() % 2 != 0)
            return "";
        for (size_t i = 0; i < input.length(); i += 2)
        {
            std::string byteString = input.substr(i, 2);
            const char byte        = static_cast<char>(strtol(byteString.c_str(), nullptr, 16));
            result += byte;
        }
        return result;
    }

    static std::string rot13(const std::string &input)
    {
        std::string result = input;
        for (char &c : result)
        {
            if (c >= 'a' && c <= 'z')
            {
                c = (c - 'a' + 13) % 26 + 'a';
            }
            else if (c >= 'A' && c <= 'Z')
            {
                c = (c - 'A' + 13) % 26 + 'A';
            }
        }
        return result;
    }

    static std::string reverse(const std::string &input)
    {
        std::string result = input;
        std::ranges::reverse(result);
        return result;
    }

    static std::string encrypt(const std::string &input, const Obfuscate obf)
    {
        switch (obf)
        {
        case Obfuscate::Base64:
            return base64_encode(input);
        case Obfuscate::Hex:
            return hex_encode(input);
        case Obfuscate::ROT13:
            return rot13(input);
        case Obfuscate::Reverse:
            return reverse(input);
        case Obfuscate::Combined:
            return reverse(base64_encode(input));
        case Obfuscate::None:
        default:
            return input;
        }
    }

    static std::string decrypt(const std::string &input, const Obfuscate obf)
    {
        switch (obf)
        {
        case Obfuscate::Base64:
            return base64_decode(input);
        case Obfuscate::Hex:
            return hex_decode(input);
        case Obfuscate::ROT13:
            return rot13(input); // ROT13 is symmetric
        case Obfuscate::Reverse:
            return reverse(input); // Reverse is symmetric
        case Obfuscate::Combined:
            return base64_decode(reverse(input));
        case Obfuscate::None:
        default:
            return input;
        }
    }
};

class PathResolver
{
  public:
    static std::string get_program_name()
    {
#if defined(_WIN32)
        char module_path[MAX_PATH]{};
        const DWORD r = GetModuleFileNameA(nullptr, module_path, MAX_PATH);
        if (r > 0 && r < MAX_PATH)
        {
            const std::filesystem::path p(module_path);
            return p.stem().string();
        }
#elif defined(__APPLE__)
        char buf[4096];
        uint32_t size = sizeof(buf);
        if (_NSGetExecutablePath(buf, &size) == 0)
        {
            std::filesystem::path p(buf);
            return p.stem().string();
        }
#elif defined(__linux__)
        char buf[PATH_MAX];
        ssize_t len = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (len > 0)
        {
            buf[len] = '\0';
            std::filesystem::path p(buf);
            return p.stem().string();
        }
#endif
        return "config_app";
    }

    static std::filesystem::path get_appdata_path()
    {
#if defined(_WIN32)
        WCHAR buff[MAX_PATH]{};
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buff)))
        {
            return std::filesystem::path(std::wstring(buff)) / get_program_name();
        }
        // Fallbacks...
        return std::filesystem::current_path();
#elif defined(__APPLE__)
        const char *home = std::getenv("HOME");
        if (home)
            return std::filesystem::path(home) / "Library" / "Application Support" / get_program_name();
        return std::filesystem::current_path();
#else
        const char *xdg = std::getenv("XDG_CONFIG_HOME");
        if (xdg)
            return std::filesystem::path(xdg) / get_program_name();
        const char *home = std::getenv("HOME");
        if (home)
            return std::filesystem::path(home) / ".config" / get_program_name();
        return std::filesystem::current_path();
#endif
    }

    static std::string resolve(const std::string &path, const Path type)
    {
        const std::filesystem::path p(path);

        switch (type)
        {
        case Path::Absolute:
            return std::filesystem::absolute(p).string();
        case Path::AppData: {
            const auto app_data = get_appdata_path();
            if (!std::filesystem::exists(app_data))
            {
                std::filesystem::create_directories(app_data);
            }
            return (app_data / p).string();
        }
        case Path::Relative:
        default:
            return std::filesystem::absolute(p).string(); // Relative to CWD, made absolute for storage
        }
    }
};

} // namespace detail

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
        if (!std::filesystem::exists(file_path_))
        {
            data_ = json::object();
            return;
        }

        try
        {
            std::ifstream file(file_path_);
            json loaded_data;
            file >> loaded_data;

            // Handle Obfuscation
            if (loaded_data.contains(META_OBFUSCATION_KEY))
            {
                auto meta = loaded_data[META_OBFUSCATION_KEY];
                for (auto &[key, val] : meta.items())
                {
                    obfuscation_map_[key] = static_cast<Obfuscate>(val.get<int>());
                }
                loaded_data.erase(META_OBFUSCATION_KEY);
            }

            // Deobfuscate values
            for (const auto &[key, type] : obfuscation_map_)
            {
                if (type == Obfuscate::None)
                    continue;

                try
                {
                    nlohmann::json::json_pointer ptr(key);
                    if (loaded_data.contains(ptr))
                    {
                        auto &val = loaded_data[ptr];
                        if (val.is_string())
                        {
                            val = detail::ObfuscationEngine::decrypt(val.get<std::string>(), type);
                        }
                    }
                    else if (loaded_data.contains(key))
                    { // Fallback for simple keys if pointer fails? No, ptr handles simple keys too (with /)
                      // Actually json_pointer constructor handles "key" as is? No, needs "/" prefix usually.
                      // But nlohmann::json::json_pointer("/key") works.
                      // Let's assume keys stored in map are valid json pointers or keys.
                      // If user passes "key", we might store it as "/key" internally or just "key".
                      // The design example shows "app/name", which implies json pointer syntax but without leading /.
                      // nlohmann::json::json_pointer needs leading /.
                    }
                }
                catch (...)
                {
                }
            }

            // For simple keys that might not be pointers, we should check.
            // But let's assume we normalize keys to pointers if they contain separators, or just use json pointer
            // access always. The previous implementation had `is_json_pointer`.

            // Re-apply deobfuscation more carefully
            // Actually, simpler approach: Just traverse the map.
            for (const auto &[key, type] : obfuscation_map_)
            {
                if (type == Obfuscate::None)
                    continue;

                // Try as JSON pointer first (prepend / if needed)
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
                    // Try as direct key if pointer fails
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

    void notify(const std::string &key, [[maybe_unused]] const json &val) const
    {
        for (const auto &l : listeners_)
        {
            // Check if listener key matches or is a parent path
            // Simple string prefix match for now, or exact match?
            // Design says: "监听路径前缀" (Listen to path prefix)
            if (key == l.key || key.find(l.key + "/") == 0)
            {
                // For path listeners, maybe we should pass the sub-object?
                // Design example: listen "user/profile", set "user/profile/name".
                // Callback gets "user/profile" value (the whole object) or the changed value?
                // Design example output: "用户资料变更: {"age":25,"name":"张三"}" when setting name.
                // So it gets the value AT the listened key.

                try
                {
                    // Get value at listener's key
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

    json get_value_at(const std::string &key_or_ptr) const
    {
        const std::string ptr_str = (key_or_ptr.front() == '/') ? key_or_ptr : "/" + key_or_ptr;
        try
        {
            return data_.at(nlohmann::json::json_pointer(ptr_str));
        }
        catch (...)
        {
            return json(); // Or throw?
        }
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
    template <typename T> T get(const std::string &key, const T &default_value) const
    {
        std::shared_lock lock(mutex_);
        if (key.find('/') == std::string::npos)
        {
            auto it = data_.find(key);
            if (it != data_.end())
            {
                try
                {
                    return it->get<T>();
                }
                catch (...)
                {
                    return default_value;
                }
            }
            return default_value;
        }

        const std::string ptr_str = (key.front() == '/') ? key : "/" + key;
        const nlohmann::json::json_pointer ptr(ptr_str);
        if (data_.contains(ptr))
        {
            try
            {
                return data_.at(ptr).get<T>();
            }
            catch (...)
            {
                return default_value;
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
     * @return The retrieved value.
     * @throws std::runtime_error If key is missing and strategy is ThrowException.
     */
    template <typename T> T get(const std::string &key) const
    {
        std::shared_lock lock(mutex_);
        if (key.find('/') == std::string::npos)
        {
            auto it = data_.find(key);
            if (it != data_.end())
            {
                return it->get<T>();
            }
            if (get_strategy_ == GetStrategy::ThrowException)
            {
                throw std::runtime_error("Key not found: " + key);
            }
            return T{};
        }

        const std::string ptr_str = (key.front() == '/') ? key : "/" + key;
        const nlohmann::json::json_pointer ptr(ptr_str);
        if (data_.contains(ptr))
        {
            return data_.at(ptr).get<T>();
        }

        if (get_strategy_ == GetStrategy::ThrowException)
        {
            throw std::runtime_error("Key not found: " + key);
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
     * @return true if the operation succeeded (including auto-save if enabled), false if auto-save failed.
     * @throws std::runtime_error If setting the value fails in memory (e.g., path conflict).
     */
    template <typename T> bool set(const std::string &key, const T &value, const Obfuscate obf = Obfuscate::None)
    {
        {
            std::unique_lock lock(mutex_);
            const std::string ptr_str = (key.front() == '/') ? key : "/" + key;
            try
            {
                data_[nlohmann::json::json_pointer(ptr_str)] = value;

                if (obf != Obfuscate::None)
                {
                    obfuscation_map_[key] = obf;
                }
                else
                {
                    obfuscation_map_.erase(key);
                }
            }
            catch (const std::exception &e)
            {
                throw std::runtime_error(std::string("Config set failed for key '") + key + "': " + e.what());
            }
        }

        notify(key, json(value)); // Notify might need lock adjustment or be careful

        if (save_strategy_ == SaveStrategy::Auto)
        {
            return save();
        }
        return true;
    }

    /**
     * @brief Removes a key and its value from the configuration.
     * @param key The configuration key or JSON Pointer path to remove.
     * @return true if the operation succeeded (including auto-save if enabled), false if auto-save failed.
     */
    bool remove(const std::string &key)
    {
        {
            std::unique_lock lock(mutex_);
            const std::string ptr_str = (key.front() == '/') ? key : "/" + key;
            try
            {
                // nlohmann::json doesn't have easy remove by pointer if it's deep?
                // actually ptr.parent_pointer() and erase
                const nlohmann::json::json_pointer ptr(ptr_str);
                if (ptr.empty())
                { // root
                    data_.clear();
                }
                else
                {
                    const auto parent_ptr = ptr.parent_pointer();
                    if (data_.contains(parent_ptr))
                    {
                        auto &parent = data_[parent_ptr];
                        parent.erase(ptr.back());
                    }
                }
                obfuscation_map_.erase(key);
            }
            catch (...)
            {
            }
        }
        if (save_strategy_ == SaveStrategy::Auto)
        {
            return save();
        }
        return true;
    }

    /**
     * @brief Checks if a key exists in the configuration.
     * @param key The configuration key or JSON Pointer path.
     * @return true if the key exists, false otherwise.
     */
    bool contains(const std::string &key) const
    {
        std::shared_lock lock(mutex_);
        const std::string ptr_str = (key.front() == '/') ? key : "/" + key;
        return data_.contains(nlohmann::json::json_pointer(ptr_str));
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
        json save_data;
        std::unordered_map<std::string, Obfuscate> obf_map_copy;

        {
            std::shared_lock lock(mutex_);
            save_data    = data_;
            obf_map_copy = obfuscation_map_;
        }

        // Ensure directory exists
        try
        {
            std::filesystem::path p(file_path_);
            if (p.has_parent_path())
            {
                std::filesystem::create_directories(p.parent_path());
            }
        }
        catch (...)
        {
            return false;
        }

        // Apply obfuscation
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
                    // Skip this key if obfuscation fails
                }
            }

            // Add meta
            json meta;
            for (const auto &[key, val] : obf_map_copy)
            {
                meta[key] = static_cast<int>(val);
            }
            save_data[META_OBFUSCATION_KEY] = meta;
        }

        try
        {
            std::ofstream file(file_path_);
            if (!file.is_open())
            {
                return false;
            }
            if (format == JsonFormat::Pretty)
            {
                file << save_data.dump(4);
            }
            else
            {
                file << save_data.dump();
            }
            return file.good();
        }
        catch (...)
        {
            return false;
        }
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
        if (save_strategy_ == SaveStrategy::Auto)
        {
            return save();
        }
        return true;
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

// 3.3 全局函数接口
namespace registry
{
inline std::unordered_map<std::string, std::shared_ptr<ConfigStore>> &get_stores()
{
    static std::unordered_map<std::string, std::shared_ptr<ConfigStore>> stores;
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
inline ConfigStore &get_store(const std::string &path, Path type = Path::Relative,
                              SaveStrategy save_strategy = SaveStrategy::Auto,
                              GetStrategy get_strategy   = GetStrategy::DefaultValue)
{
    std::lock_guard<std::mutex> lock(registry::get_mutex());
    auto &stores          = registry::get_stores();
    const std::string key = path; // Simplification, unique by path string
    if (!stores.contains(key))
    {
        stores[key] = std::make_shared<ConfigStore>(path, type, save_strategy, get_strategy);
    }
    return *stores[key];
}

// Global default store helper (internal)
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
template <typename T> T get(const std::string &key, const T &default_value)
{
    return get_default_store().get<T>(key, default_value);
}
/**
 * @brief Global convenience function: Gets a value from the default store.
 */
template <typename T> T get(const std::string &key)
{
    return get_default_store().get<T>(key);
}
/**
 * @brief Global convenience function: Sets a value in the default store.
 */
template <typename T> bool set(const std::string &key, const T &value, Obfuscate obf = Obfuscate::None)
{
    return get_default_store().set(key, value, obf);
}
/**
 * @brief Global convenience function: Removes a key from the default store.
 */
inline bool remove(const std::string &key)
{
    return get_default_store().remove(key);
}
/**
 * @brief Global convenience function: Checks if a key exists in the default store.
 */
inline bool contains(const std::string &key)
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
