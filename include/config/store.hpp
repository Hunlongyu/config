#pragma once

#include <atomic>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include <config/detail/obfuscation.hpp>
#include <config/detail/path_resolver.hpp>
#include <config/detail/types.hpp>

namespace config
{

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
            try
            {
                return data_.get<T>();
            }
            catch (const nlohmann::json::exception &)
            {
                return default_value;
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
                // Fallthrough to return default_value
            }
        }
        return default_value;
    }

    /**
     * @brief Retrieves a value from the configuration.
     *
     * - For non-empty keys: behavior depends on GetStrategy when key is missing.
     *   - DefaultValue: Returns T{}
     *   - ThrowException: Throws std::runtime_error
     * - For empty key (""): attempts to deserialize the entire root JSON into T.
     *   - On success: returns the deserialized value (regardless of strategy).
     *   - On failure: DefaultValue returns T{}; ThrowException throws std::runtime_error.
     *
     * @tparam T Type to deserialize into.
     * @param key The configuration key / JSON Pointer path, or "" for root.
     * @param location Source location for diagnostics.
     * @return The retrieved value, or T{} on failure with DefaultValue strategy.
     * @throws std::runtime_error On deserialization failure with ThrowException strategy.
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
            catch (const nlohmann::json::exception &e)
            {
                if (get_strategy_ == GetStrategy::ThrowException)
                {
                    throw std::runtime_error(std::format("Root conversion failed: {} ({}:{}:{})", e.what(),
                                                         location.file_name(), location.line(),
                                                         location.function_name()));
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
            std::string error_msg;
            {
                std::unique_lock lock(mutex_);
                try
                {
                    nlohmann::json new_root = nlohmann::json(value);
                    if (!new_root.is_object())
                    {
                        return false;
                    }
                    data_ = std::move(new_root);
                    obfuscation_map_.clear();
                }
                catch (const std::exception &e)
                {
                    error_msg = std::format("Config set root failed: {} ({}:{}:{})", e.what(), location.file_name(),
                                            location.line(), location.function_name());
                }
            }
            if (!error_msg.empty())
            {
                throw std::runtime_error(error_msg);
            }
            if (save_strategy_ == SaveStrategy::Auto)
            {
                return save();
            }
            return true;
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
        std::shared_lock lock(mutex_);
        if (key.empty())
        {
            return !data_.empty();
        }
        const std::string ptr_str = (key.front() == '/') ? std::string(key) : "/" + std::string(key);
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
