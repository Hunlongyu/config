#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
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
#include <thread>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

#include <nlohmann/json.hpp>

#include <config/detail/obfuscation.hpp>
#include <config/detail/path_resolver.hpp>
#include <config/detail/types.hpp>

namespace config
{

// Forward declaration for RAII connection handle.
class Connection;

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
 * @brief Options bundle for constructing a ConfigStore.
 *
 * Aggregates all per-store settings so callers can use named fields instead
 * of a positional four-argument constructor.
 */
struct StoreOptions
{
    Path path_type              = Path::Relative;
    SaveStrategy save           = SaveStrategy::Auto;
    MissingKeyPolicy on_missing = MissingKeyPolicy::DefaultValue;
    JsonFormat format           = JsonFormat::Pretty;
    std::string env_prefix; // empty = no env var override; used in B13
};

/**
 * @brief Exception thrown when an auto-save disk write fails in set/remove/clear.
 */
class SaveError : public std::runtime_error
{
  public:
    using std::runtime_error::runtime_error;
};

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
    using ListenerCallback = std::function<void(const json &)>;
    using ListenerId       = size_t;

  private:
    std::string file_path_;
    Path path_type_;
    SaveStrategy save_strategy_;
    MissingKeyPolicy missing_key_policy_;
    JsonFormat json_format_ = JsonFormat::Pretty;
    StoreOptions opts_;

    mutable std::shared_mutex mutex_;
    json data_;
    std::unordered_map<std::string, Encoding> obfuscation_map_;

    struct Listener
    {
        ListenerId id;
        std::string key;
        ListenerCallback callback;
    };

    std::vector<Listener> listeners_;
    std::atomic<size_t> next_listener_id_{1};

    std::thread watcher_thread_;
    std::atomic<bool> watch_active_{false};
    std::filesystem::file_time_type last_write_time_;

    std::function<void(const json &)> validator_;

    static constexpr const char *META_OBFUSCATION_KEY = "__obfuscate_meta__";

    static void deep_merge(json &base, const json &overlay)
    {
        if (base.is_object() && overlay.is_object())
        {
            for (const auto &[key, val] : overlay.items())
            {
                if (base.contains(key) && base[key].is_object() && val.is_object())
                {
                    deep_merge(base[key], val);
                }
                else
                {
                    base[key] = val;
                }
            }
        }
        else
        {
            base = overlay;
        }
    }

    void apply_single_env(const std::string &entry)
    {
        const auto eq = entry.find('=');
        if (eq == std::string::npos)
            return;
        const std::string name  = entry.substr(0, eq);
        const std::string value = entry.substr(eq + 1);

        if (name.size() <= opts_.env_prefix.size())
            return;
        if (name.substr(0, opts_.env_prefix.size()) != opts_.env_prefix)
            return;

        // Strip prefix, lowercase, replace _ with /
        std::string key = name.substr(opts_.env_prefix.size());
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        std::replace(key.begin(), key.end(), '_', '/');

        // Try to parse value as JSON; fallback to string
        nlohmann::json jval;
        try
        {
            jval = nlohmann::json::parse(value);
        }
        catch (...)
        {
            jval = value;
        }

        const std::string ptr_str = "/" + key;
        try
        {
            data_[nlohmann::json::json_pointer(ptr_str)] = jval;
        }
        catch (...)
        {
        }
    }

    void apply_env_overrides()
    {
        if (opts_.env_prefix.empty())
            return;
#if defined(_WIN32)
        LPCH env = GetEnvironmentStrings();
        if (!env)
            return;
        for (LPCH p = env; *p; p += strlen(p) + 1)
        {
            std::string entry(p);
            apply_single_env(entry);
        }
        FreeEnvironmentStrings(env);
#else
        extern char **environ;
        for (char **ep = environ; *ep; ++ep)
        {
            apply_single_env(std::string(*ep));
        }
#endif
    }

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
                        obfuscation_map_[key] = static_cast<Encoding>(val.get<int>());
                    }
                    loaded_data.erase(META_OBFUSCATION_KEY);
                }

                for (const auto &[key, type] : obfuscation_map_)
                {
                    if (type == Encoding::None)
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
        apply_env_overrides();
        if (validator_)
        {
            validator_(data_); // throws on invalid — let the exception propagate
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
     * @param missing_key_policy Strategy for handling missing keys (DefaultValue or ThrowException).
     */
    ConfigStore(const std::string &path, const Path type = Path::Relative,
                const SaveStrategy save_strategy          = SaveStrategy::Auto,
                const MissingKeyPolicy missing_key_policy = MissingKeyPolicy::DefaultValue)
        : path_type_(type), save_strategy_(save_strategy), missing_key_policy_(missing_key_policy)
    {
        file_path_ = detail::PathResolver::resolve(path, type);
        load();
    }

    /**
     * @brief Constructs a new ConfigStore instance using a StoreOptions bundle.
     *
     * @param path File path for the configuration file.
     * @param opts Options struct specifying path type, save strategy, missing key
     *             policy, JSON format, and optional environment-variable prefix.
     */
    explicit ConfigStore(const std::string &path, StoreOptions opts)
        : path_type_(opts.path_type), save_strategy_(opts.save), missing_key_policy_(opts.on_missing),
          json_format_(opts.format), opts_(opts)
    {
        file_path_ = detail::PathResolver::resolve(path, opts.path_type);
        load();
    }

    ConfigStore(const ConfigStore &)            = delete;
    ConfigStore &operator=(const ConfigStore &) = delete;
    ConfigStore(ConfigStore &&)                 = delete;
    ConfigStore &operator=(ConfigStore &&)      = delete;

    ~ConfigStore()
    {
        stop_watch();
    }

    /**
     * @brief Gets the absolute path to the configuration file.
     * @return Absolute file path string.
     */
    std::string get_store_path() const
    {
        return file_path_;
    }

    /**
     * @brief Gets the Path type used when constructing this store.
     * @return The Path enum value (e.g., Relative, Absolute, AppData).
     */
    Path path_type() const
    {
        return path_type_;
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
     * @brief Sets the policy for handling missing keys.
     * @param policy The new MissingKeyPolicy (DefaultValue or ThrowException).
     */
    void set_missing_key_policy(const MissingKeyPolicy policy)
    {
        std::unique_lock lock(mutex_);
        missing_key_policy_ = policy;
    }

    /**
     * @brief Gets the current missing key policy.
     * @return Current MissingKeyPolicy.
     */
    MissingKeyPolicy missing_key_policy() const
    {
        std::shared_lock lock(mutex_);
        return missing_key_policy_;
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
     * @brief Registers a validator callback invoked after every load()/reload().
     *
     * The callable receives the full JSON object after data is loaded and env
     * overrides are applied. Throw from the validator to reject invalid config;
     * the exception propagates to the caller of load()/reload().
     *
     * @param validator Callable with signature void(const json&).
     */
    void set_validator(std::function<void(const json &)> validator)
    {
        std::unique_lock lock(mutex_);
        validator_ = std::move(validator);
    }

    /**
     * @brief Removes the previously registered validator callback.
     */
    void clear_validator()
    {
        std::unique_lock lock(mutex_);
        validator_ = nullptr;
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
     * - For non-empty keys: behavior depends on MissingKeyPolicy when key is missing.
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
                if (missing_key_policy_ == MissingKeyPolicy::ThrowException)
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

        if (missing_key_policy_ == MissingKeyPolicy::ThrowException)
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
     * @param encoding Encoding method to apply (optional).
     * @param location
     * @throws std::invalid_argument If key is "" and value is not a JSON object type.
     * @throws std::runtime_error If setting the value fails in memory (e.g., path conflict).
     * @throws SaveError If auto-save is enabled and the disk write fails.
     */
    template <typename T>
        requires JsonWritable<T>
    void set(std::string_view key, const T &value, const Encoding encoding = Encoding::None,
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
                        throw std::invalid_argument("set(\"\") requires a JSON object type");
                    }
                    data_ = std::move(new_root);
                    obfuscation_map_.clear();
                }
                catch (const std::invalid_argument &)
                {
                    throw;
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
                if (!save())
                {
                    throw SaveError("Save failed for key '': disk write error");
                }
            }
            return;
        }

        std::string error_msg;
        {
            std::unique_lock lock(mutex_);
            const std::string ptr_str = (key.front() == '/') ? std::string(key) : "/" + std::string(key);
            try
            {
                data_[nlohmann::json::json_pointer(ptr_str)] = value;

                if (encoding != Encoding::None)
                {
                    obfuscation_map_[std::string(key)] = encoding;
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

        if (save_strategy_ == SaveStrategy::Auto)
        {
            if (!save())
            {
                throw SaveError("Save failed for key '" + std::string(key) + "': disk write error");
            }
        }
    }

    /**
     * @brief Retrieves the root JSON object as type T, with a default fallback.
     * @tparam T Type to deserialize the root into.
     * @param default_value The value to return if deserialization fails.
     * @return The deserialized root value or default_value.
     */
    template <typename T>
        requires JsonReadable<T>
    T get_root(const T &default_value) const
    {
        return get<T>("", default_value);
    }

    /**
     * @brief Retrieves the root JSON object as type T.
     * @tparam T Type to deserialize the root into.
     * @param location Source location for diagnostics.
     * @return The deserialized root value, or T{} on failure with DefaultValue strategy.
     * @throws std::runtime_error On deserialization failure with ThrowException strategy.
     */
    template <typename T>
        requires JsonReadable<T>
    T get_root(const std::source_location location = std::source_location::current()) const
    {
        return get<T>("", location);
    }

    /**
     * @brief Replaces the entire root JSON object with the given value.
     * @tparam T Type of the value to set as root.
     * @param value The value to store as the new root.
     */
    template <typename T>
        requires JsonWritable<T>
    void set_root(const T &value)
    {
        set<T>("", value);
    }

    /**
     * @brief Returns the configuration file path as a std::filesystem::path.
     * @return Absolute file path.
     */
    std::filesystem::path path() const
    {
        return std::filesystem::path(file_path_);
    }

    /**
     * @brief Removes a key and its value from the configuration.
     * @param key The configuration key or JSON Pointer path to remove.
     * @throws SaveError If auto-save is enabled and the disk write fails.
     */
    void remove(std::string_view key)
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
        if (save_strategy_ == SaveStrategy::Auto)
        {
            if (!save())
            {
                throw SaveError("Remove: disk write error");
            }
        }
    }

    /**
     * @brief Checks if a key exists in the configuration.
     * @param key The configuration key or JSON Pointer path.
     * @return true if the key exists, false otherwise.
     */
    [[nodiscard]] bool contains(std::string_view key) const
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
    [[nodiscard]] bool save() const
    {
        return save(json_format_);
    }

    /**
     * @brief Saves the current configuration to disk using a specific format.
     * @param format The output format (Pretty or Compact).
     * @return true if saved successfully, false otherwise.
     */
    [[nodiscard]] bool save(JsonFormat format) const
    {
        bool result = false;
        json save_data;
        std::unordered_map<std::string, Encoding> obf_map_copy;

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
                    if (type == Encoding::None)
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
     * @throws SaveError If auto-save is enabled and the disk write fails.
     */
    void clear()
    {
        {
            std::unique_lock lock(mutex_);
            data_.clear();
            obfuscation_map_.clear();
        }
        if (save_strategy_ == SaveStrategy::Auto)
        {
            if (!save())
            {
                throw SaveError("Clear: disk write error");
            }
        }
    }

    /**
     * @brief Connects a listener callback to a specific key.
     *
     * The callback will be invoked whenever the value of the key (or its children) changes.
     *
     * @param key The key to listen to.
     * @param callback Function to call on change.
     * @return A RAII Connection handle that auto-disconnects on destruction.
     */
    Connection connect(const std::string &key, const std::function<void(const json &)> &callback);

    /**
     * @brief Connects a typed listener callback to a specific key.
     *
     * Wraps connect() with automatic JSON deserialization to type T.
     * Deserialization errors are silently ignored.
     *
     * @tparam T Type to deserialize the JSON value into.
     * @param key The key to listen to.
     * @param callback Function to call on change, receiving the value as T.
     * @return A RAII Connection handle that auto-disconnects on destruction.
     */
    template <typename T>
        requires JsonReadable<T>
    Connection on_change(const std::string &key, std::function<void(const T &)> callback);

    /**
     * @brief Atomically reads a key or initializes it with a default value.
     *
     * If the key already exists and can be decoded as T, returns the stored value.
     * Otherwise writes default_value for the key and returns it.
     * When SaveStrategy is Auto the store is saved after initialization.
     *
     * @tparam T Type to read/write (must satisfy JsonReadable and JsonWritable).
     * @param key The configuration key or JSON Pointer path.
     * @param default_value Value to store and return if the key is absent or unreadable.
     * @return The existing value, or default_value after initialization.
     * @throws SaveError If auto-save is enabled and the disk write fails.
     */
    template <typename T>
        requires JsonReadable<T> && JsonWritable<T>
    T get_or_set(std::string_view key, const T &default_value)
    {
        std::unique_lock lock(mutex_);
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
            }
        }
        data_[ptr] = default_value;
        if (save_strategy_ == SaveStrategy::Auto)
        {
            lock.unlock();
            if (!save())
                throw SaveError("get_or_set: auto-save failed for key '" + std::string(key) + "'");
        }
        return default_value;
    }

    /**
     * @brief Starts a background thread that polls the config file every interval ms.
     *
     * If the file's modification time changes, reload() is called automatically.
     * Calling start_watch() while already watching has no effect.
     *
     * @param interval Polling interval (default: 1000 ms).
     */
    void start_watch(std::chrono::milliseconds interval = std::chrono::milliseconds{1000})
    {
        if (watch_active_.exchange(true))
            return; // already watching
        if (std::filesystem::exists(file_path_))
        {
            last_write_time_ = std::filesystem::last_write_time(file_path_);
        }
        watcher_thread_ = std::thread([this, interval]() {
            while (watch_active_)
            {
                std::this_thread::sleep_for(interval);
                if (!watch_active_)
                    break;
                try
                {
                    if (std::filesystem::exists(file_path_))
                    {
                        auto mtime = std::filesystem::last_write_time(file_path_);
                        if (mtime != last_write_time_)
                        {
                            last_write_time_ = mtime;
                            reload();
                        }
                    }
                }
                catch (...)
                {
                }
            }
        });
    }

    /**
     * @brief Stops the background file-watcher thread, if running.
     *
     * Blocks until the watcher thread has exited.
     */
    void stop_watch()
    {
        watch_active_ = false;
        if (watcher_thread_.joinable())
            watcher_thread_.join();
    }

    /**
     * @brief Disconnects a listener by its ID.
     * @param connection_id The ID returned by connect() / held by a Connection.
     */
    void disconnect(size_t connection_id)
    {
        std::unique_lock lock(mutex_);
        listeners_.erase(std::remove_if(listeners_.begin(), listeners_.end(),
                                        [connection_id](const Listener &l) { return l.id == connection_id; }),
                         listeners_.end());
    }

    /**
     * @brief Returns all immediate child keys at the top level or under a given prefix.
     *
     * @param prefix Empty string for root-level keys, or a key / JSON Pointer path
     *               designating a sub-object.
     * @return Vector of child key names (not full paths).
     */
    std::vector<std::string> keys(std::string_view prefix = "") const
    {
        std::shared_lock lock(mutex_);
        std::vector<std::string> result;
        if (prefix.empty())
        {
            if (data_.is_object())
            {
                for (const auto &[k, _] : data_.items())
                    result.push_back(k);
            }
        }
        else
        {
            const std::string ptr_str = (prefix.front() == '/') ? std::string(prefix) : "/" + std::string(prefix);
            try
            {
                const auto &node = data_.at(nlohmann::json::json_pointer(ptr_str));
                if (node.is_object())
                {
                    for (const auto &[k, _] : node.items())
                        result.push_back(k);
                }
            }
            catch (...)
            {
            }
        }
        return result;
    }

    /**
     * @brief Alias for keys() — returns immediate child key names under prefix.
     *
     * The name emphasises subtree traversal; behaviour is identical to keys().
     *
     * @param prefix Empty string for root-level keys, or a key / JSON Pointer path.
     * @return Vector of child key names.
     */
    std::vector<std::string> children(std::string_view prefix = "") const
    {
        return keys(prefix);
    }

    /**
     * @brief Deep-merges a JSON object into the current configuration data.
     *
     * Nested objects are merged recursively; scalar and array values are
     * overwritten by the overlay.  Requires a JSON object at the top level.
     *
     * @param overlay JSON object whose keys override or extend current data.
     * @throws std::invalid_argument If overlay is not a JSON object.
     * @throws SaveError If SaveStrategy is Auto and the disk write fails.
     */
    void merge(const json &overlay)
    {
        if (!overlay.is_object())
            throw std::invalid_argument("merge() requires a JSON object");
        {
            std::unique_lock lock(mutex_);
            deep_merge(data_, overlay);
        }
        if (save_strategy_ == SaveStrategy::Auto)
        {
            if (!save())
                throw SaveError("merge: auto-save failed");
        }
    }

    /**
     * @brief Loads a JSON file from disk and deep-merges it into current data.
     *
     * @param path File path to load.
     * @param type Strategy for resolving the file path.
     * @throws std::runtime_error If the file does not exist.
     * @throws SaveError If SaveStrategy is Auto and the disk write fails.
     */
    void merge_file(const std::string &path, Path type = Path::Relative)
    {
        const std::string abs_path = detail::PathResolver::resolve(path, type);
        if (!std::filesystem::exists(abs_path))
            throw std::runtime_error("merge_file: file not found: " + abs_path);
        json overlay;
        {
            std::ifstream f(abs_path);
            f >> overlay;
        }
        merge(overlay);
    }

    /**
     * @brief Loads multiple JSON files in order, merging each into the store.
     *
     * Files are processed left-to-right; later files have higher priority.
     * Non-existent files are silently skipped.  Parse errors are also silently
     * ignored so a corrupt optional layer does not block loading.
     *
     * @param paths Ordered list of file paths.
     * @param type  Strategy for resolving each path.
     * @throws SaveError If SaveStrategy is Auto and the disk write fails.
     */
    void load_layered(const std::vector<std::string> &paths, Path type = Path::Relative)
    {
        for (const auto &p : paths)
        {
            const std::string abs = detail::PathResolver::resolve(p, type);
            if (!std::filesystem::exists(abs))
                continue;
            try
            {
                json layer_data;
                std::ifstream f(abs);
                f >> layer_data;
                {
                    std::unique_lock lock(mutex_);
                    deep_merge(data_, layer_data);
                }
            }
            catch (...)
            {
            }
        }
        if (save_strategy_ == SaveStrategy::Auto)
        {
            if (!save())
                throw SaveError("load_layered: auto-save failed");
        }
    }
};

// Connection must not outlive the ConfigStore it was created from.
class Connection
{
    ConfigStore *store_{nullptr};
    size_t id_{0};
    bool active_{false};

  public:
    Connection() = default;
    Connection(ConfigStore &store, size_t id) : store_(&store), id_(id), active_(true)
    {
    }
    ~Connection()
    {
        disconnect();
    }
    Connection(const Connection &)            = delete;
    Connection &operator=(const Connection &) = delete;
    Connection(Connection &&o) noexcept : store_(o.store_), id_(o.id_), active_(o.active_)
    {
        o.active_ = false;
    }
    Connection &operator=(Connection &&o) noexcept
    {
        if (this != &o)
        {
            disconnect();
            store_    = o.store_;
            id_       = o.id_;
            active_   = o.active_;
            o.active_ = false;
        }
        return *this;
    }
    void disconnect()
    {
        if (active_ && store_)
        {
            store_->disconnect(id_);
            active_ = false;
        }
    }
    size_t id() const
    {
        return id_;
    }
    bool connected() const
    {
        return active_;
    }
};

inline Connection ConfigStore::connect(const std::string &key, const std::function<void(const json &)> &callback)
{
    std::unique_lock lock(mutex_);
    const size_t id = next_listener_id_++;
    listeners_.push_back({id, key, callback});
    return Connection(*this, id);
}

template <typename T>
    requires JsonReadable<T>
inline Connection ConfigStore::on_change(const std::string &key, std::function<void(const T &)> callback)
{
    return connect(key, [cb = std::move(callback)](const nlohmann::json &j) {
        try
        {
            cb(j.get<T>());
        }
        catch (...)
        {
        }
    });
}

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
 * @param missing_key_policy Strategy for handling missing keys.
 * @return Reference to the ConfigStore instance.
 */
inline ConfigStore &get_store(std::string_view path, Path type = Path::Relative,
                              SaveStrategy save_strategy          = SaveStrategy::Auto,
                              MissingKeyPolicy missing_key_policy = MissingKeyPolicy::DefaultValue)
{
    std::lock_guard<std::mutex> lock(registry::get_mutex());
    auto &stores = registry::get_stores();
    auto it      = stores.find(path);
    if (it == stores.end())
    {
        std::string path_str(path);
        auto [new_it, inserted] =
            stores.emplace(path_str, std::make_shared<ConfigStore>(path_str, type, save_strategy, missing_key_policy));
        it = new_it;
    }
    else
    {
        if (it->second->path_type() != type)
        {
            throw std::logic_error(std::format("get_store(\"{}\") already cached with a different path type", path));
        }
    }
    return *it->second;
}

/**
 * @brief Retrieves or creates a ConfigStore instance using a StoreOptions bundle.
 *
 * Instances are cached by path. If an entry already exists the cached instance
 * is returned unchanged (options are only applied on first construction).
 *
 * @param path File path for the configuration.
 * @param opts Options struct specifying all store settings.
 * @return Reference to the ConfigStore instance.
 */
inline ConfigStore &get_store(std::string_view path, StoreOptions opts)
{
    std::lock_guard<std::mutex> lock(registry::get_mutex());
    auto &stores = registry::get_stores();
    auto it      = stores.find(path);
    if (it == stores.end())
    {
        std::string path_str(path);
        auto [new_it, inserted] = stores.emplace(path_str, std::make_shared<ConfigStore>(path_str, opts));
        it                      = new_it;
    }
    return *it->second;
}

inline ConfigStore &get_default_store()
{
    return get_store("config.json");
}

/**
 * @brief Removes a specific store from the global registry by its file path.
 *
 * After this call the shared_ptr for that store is released; any existing
 * references to the store obtained before this call remain valid until they
 * go out of scope.
 *
 * @param path The file path key used when the store was registered.
 * @return true if the entry was found and removed, false if not present.
 */
inline bool release_store(std::string_view path)
{
    std::lock_guard<std::mutex> lock(registry::get_mutex());
    return registry::get_stores().erase(std::string(path)) > 0;
}

/**
 * @brief Removes all stores from the global registry.
 *
 * Existing references to previously-retrieved stores remain valid until they
 * go out of scope.
 */
inline void release_all_stores()
{
    std::lock_guard<std::mutex> lock(registry::get_mutex());
    registry::get_stores().clear();
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
 * @brief Global convenience function: Sets missing key policy for the default store.
 */
inline void set_missing_key_policy(const MissingKeyPolicy policy)
{
    get_default_store().set_missing_key_policy(policy);
}
/**
 * @brief Global convenience function: Gets missing key policy of the default store.
 */
inline MissingKeyPolicy missing_key_policy()
{
    return get_default_store().missing_key_policy();
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
template <typename T> void set(std::string_view key, const T &value, Encoding encoding = Encoding::None)
{
    get_default_store().set(key, value, encoding);
}
/**
 * @brief Global convenience function: Atomically reads or initializes a key in the default store.
 */
template <typename T> inline T get_or_set(std::string_view key, const T &dv)
{
    return get_default_store().get_or_set<T>(key, dv);
}
/**
 * @brief Global convenience function: Removes a key from the default store.
 */
inline void remove(std::string_view key)
{
    get_default_store().remove(key);
}
/**
 * @brief Global convenience function: Checks if a key exists in the default store.
 */
[[nodiscard]] inline bool contains(std::string_view key)
{
    return get_default_store().contains(key);
}

/**
 * @brief Global convenience function: Saves the default store to disk.
 */
[[nodiscard]] inline bool save()
{
    return get_default_store().save();
}
/**
 * @brief Global convenience function: Saves the default store to disk with format.
 */
[[nodiscard]] inline bool save(const JsonFormat format)
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
inline void clear()
{
    get_default_store().clear();
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

/**
 * @brief Global convenience function: Gets the root of the default store as T, with a default fallback.
 */
template <typename T> inline T get_root(const T &dv)
{
    return get_default_store().get_root<T>(dv);
}
/**
 * @brief Global convenience function: Gets the root of the default store as T.
 */
template <typename T> inline T get_root()
{
    return get_default_store().get_root<T>();
}
/**
 * @brief Global convenience function: Sets the root of the default store.
 */
template <typename T> inline void set_root(const T &v)
{
    get_default_store().set_root(v);
}
/**
 * @brief Global convenience function: Gets the file path of the default store as std::filesystem::path.
 */
inline std::filesystem::path path()
{
    return get_default_store().path();
}

/**
 * @brief Global convenience function: Returns immediate child key names from the default store.
 */
inline std::vector<std::string> keys(std::string_view prefix = "")
{
    return get_default_store().keys(prefix);
}
/**
 * @brief Global convenience function: Alias for keys() on the default store.
 */
inline std::vector<std::string> children(std::string_view prefix = "")
{
    return get_default_store().children(prefix);
}

/**
 * @brief Global convenience function: Registers a validator on the default store.
 */
inline void set_validator(std::function<void(const ConfigStore::json &)> v)
{
    get_default_store().set_validator(std::move(v));
}
/**
 * @brief Global convenience function: Removes the validator from the default store.
 */
inline void clear_validator()
{
    get_default_store().clear_validator();
}

/**
 * @brief Global convenience function: Deep-merges a JSON object into the default store.
 */
inline void merge(const nlohmann::json &overlay)
{
    get_default_store().merge(overlay);
}

/**
 * @brief Global convenience function: Loads a JSON file and merges it into the default store.
 */
inline void merge_file(const std::string &path, Path type = Path::Relative)
{
    get_default_store().merge_file(path, type);
}

} // namespace config
