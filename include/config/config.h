#pragma once

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <ostream>
#include <ranges>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#if defined(_WIN32)
#include <windows.h>
#endif
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#endif
#if defined(__linux__)
#include <limits.h>
#include <unistd.h>
#endif

namespace config
{

using json   = nlohmann::json;
namespace fs = std::filesystem;

// 异常类
class config_exception : public std::exception
{
  public:
    explicit config_exception(const std::string &message) : message_(message)
    {
    }
    const char *what() const noexcept override
    {
        return message_.c_str();
    }

  private:
    std::string message_;
};

// 路径策略枚举
enum class Path
{
    CurrentDir, // 当前目录/程序名/
    AppData,    // 系统AppData目录/程序名/
    AutoDetect  // 自动检测（优先AppData，fallback到当前目录）
};

// 混淆策略枚举
enum class Obfuscate
{
    None,      // 不混淆
    Base64,    // Base64编码
    XorCipher, // XOR异或混淆
    CharShift, // 字符位移混淆
    Combined   // 组合混淆（Base64 + XOR + 位移）
};

// 混淆工具类
class obfuscation_utils
{
    static constexpr uint8_t XOR_KEY  = 0xAB;
    static constexpr int SHIFT_OFFSET = 7;

    // Base64编码表
    static constexpr char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  public:
    // Base64编码
    static std::string base64_encode(const std::string &input)
    {
        std::string encoded;
        int val = 0, bit_count = -6;
        for (const unsigned char c : input)
        {
            val = (val << 8) + c;
            bit_count += 8;
            while (bit_count >= 0)
            {
                encoded.push_back(base64_chars[(val >> bit_count) & 0x3F]);
                bit_count -= 6;
            }
        }
        if (bit_count > -6)
            encoded.push_back(base64_chars[((val << 8) >> (bit_count + 8)) & 0x3F]);
        while (encoded.size() % 4)
            encoded.push_back('=');
        return encoded;
    }

    // Base64解码
    static std::string base64_decode(const std::string &input)
    {
        std::string decoded;
        std::vector<int> T(128, -1);
        for (int i = 0; i < 64; i++)
            T[base64_chars[i]] = i;

        int val = 0, bit_count = -8;
        for (const unsigned char c : input)
        {
            if (std::isspace(c))
                continue;
            if (c == '=')
                break;
            if (c < 128 && T[c] != -1)
            {
                val = (val << 6) + T[c];
                bit_count += 6;
                if (bit_count >= 0)
                {
                    decoded.push_back(static_cast<char>((val >> bit_count) & 0xFF));
                    bit_count -= 8;
                }
            }
        }
        return decoded;
    }

    // XOR异或混淆
    static std::string xor_obfuscate(const std::string &input)
    {
        std::string result = input;
        for (size_t i = 0; i < result.length(); ++i)
        {
            result[i] ^= XOR_KEY ^ (i % 256);
        }
        return result;
    }

    // 字符位移混淆
    static std::string char_shift_obfuscate(const std::string &input)
    {
        std::string result = input;
        for (size_t i = 0; i < result.length(); ++i)
        {
            result[i] = static_cast<char>((static_cast<unsigned char>(result[i]) + SHIFT_OFFSET + (i % 10)) % 256);
        }
        return result;
    }

    // 字符位移解混淆
    static std::string char_shift_deobfuscate(const std::string &input)
    {
        std::string result = input;
        for (size_t i = 0; i < result.length(); ++i)
        {
            result[i] =
                static_cast<char>((static_cast<unsigned char>(result[i]) - SHIFT_OFFSET - (i % 10) + 256) % 256);
        }
        return result;
    }

    // 混淆函数
    static std::string obfuscate(const std::string &input, Obfuscate policy)
    {
        switch (policy)
        {
        case Obfuscate::None:
            return input;
        case Obfuscate::Base64:
            return base64_encode(input);
        case Obfuscate::XorCipher:
            return base64_encode(xor_obfuscate(input));
        case Obfuscate::CharShift:
            return base64_encode(char_shift_obfuscate(input));
        case Obfuscate::Combined:
            return base64_encode(xor_obfuscate(char_shift_obfuscate(input)));
        default:
            return input;
        }
    }

    // 解混淆函数
    static std::string deobfuscate(const std::string &input, Obfuscate policy)
    {
        switch (policy)
        {
        case Obfuscate::None:
            return input;
        case Obfuscate::Base64:
            return base64_decode(input);
        case Obfuscate::XorCipher:
            return xor_obfuscate(base64_decode(input));
        case Obfuscate::CharShift:
            return char_shift_deobfuscate(base64_decode(input));
        case Obfuscate::Combined:
            return char_shift_deobfuscate(xor_obfuscate(base64_decode(input)));
        default:
            return input;
        }
    }
};

// 路径管理器 - 支持多种路径策略（使用存储名作为目录名）
class path_manager
{
  public:
    static std::string get_program_name()
    {
#if defined(_WIN32)
        char module_path[MAX_PATH]{};
        const DWORD r = GetModuleFileNameA(nullptr, module_path, MAX_PATH);
        if (r > 0 && r < MAX_PATH)
        {
            const fs::path p(module_path);
            return p.stem().string();
        }
#elif defined(__APPLE__)
        char buf[4096];
        uint32_t size = sizeof(buf);
        if (_NSGetExecutablePath(buf, &size) == 0)
        {
            fs::path p(buf);
            return p.stem().string();
        }
#elif defined(__linux__)
        char buf[PATH_MAX];
        ssize_t len = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (len > 0)
        {
            buf[len] = '\0';
            fs::path p(buf);
            return p.stem().string();
        }
#endif
        return std::string("config_app");
    }
    static fs::path get_appdata_directory()
    {
        const char *p = nullptr;
#if defined(_WIN32)
        p = std::getenv("LOCALAPPDATA");
        if (p && *p)
        {
            return fs::path(p) / get_program_name();
        }
        const char *user = std::getenv("USERPROFILE");
        if (user && *user)
        {
            return fs::path(user) / "AppData" / "Local" / get_program_name();
        }
        return fs::current_path();
#elif defined(__APPLE__)
        const char *home = std::getenv("HOME");
        if (home && *home)
        {
            return fs::path(home) / "Library" / "Application Support" / get_program_name();
        }
        return fs::current_path();
#else
        const char *xdg = std::getenv("XDG_CONFIG_HOME");
        if (xdg && *xdg)
        {
            return fs::path(xdg) / get_program_name();
        }
        const char *home = std::getenv("HOME");
        if (home && *home)
        {
            return fs::path(home) / ".config" / get_program_name();
        }
        return fs::current_path();
#endif
    }

    static fs::path get_current_dir_config()
    {
        return fs::current_path();
    }

    static fs::path get_config_directory(Path policy = Path::AutoDetect)
    {
        switch (policy)
        {
        case Path::CurrentDir:
            return get_current_dir_config();

        case Path::AppData:
            return get_appdata_directory();

        case Path::AutoDetect:
        default: {
            auto appdata_config = get_appdata_directory();
            try
            {
                fs::create_directories(appdata_config);
                return appdata_config;
            }
            catch (...)
            {
            }
            auto current_config = get_current_dir_config();
            try
            {
                fs::create_directories(current_config);
                return current_config;
            }
            catch (...)
            {
            }
            return fs::current_path();
        }
        }
    }

    static fs::path get_config_path(const std::string &store_name, Path policy = Path::AutoDetect)
    {
        return resolve_config_file(store_name, get_config_directory(policy));
    }

    static std::string normalize_store_name(const std::string &name)
    {
#if defined(_WIN32)
        return name;
#else
        std::string n = name;
        for (auto &ch : n)
        {
            if (ch == '\\')
                ch = '/';
        }
        return n;
#endif
    }

    static fs::path resolve_config_file(const std::string &store_name, const fs::path &base_dir)
    {
        const std::string norm = normalize_store_name(store_name);
        fs::path rel(norm);
        if (rel.is_absolute())
        {
            rel = rel.relative_path();
        }
        fs::path filtered;
        for (const auto &part : rel)
        {
            auto s = part.string();
            if (s == "." || s == "..")
                continue;
            filtered /= part;
        }
        const fs::path dir = base_dir / filtered.parent_path();
        fs::path file      = dir / (filtered.filename().string() + ".json");
        return file;
    }
};

// 监听器管理
using listener_id       = uint64_t;
using listener_callback = std::function<void(const json &, const json &)>;

class listener_manager
{
    std::unordered_map<listener_id, listener_callback> listeners_;
    std::unordered_map<std::string, std::vector<listener_id>> path_listeners_;
    std::atomic<listener_id> next_id_{1};
    mutable std::shared_mutex mutex_;

  public:
    listener_id add_listener(const std::string &path, listener_callback callback)
    {
        std::unique_lock lock(mutex_);
        const auto id  = next_id_++;
        listeners_[id] = std::move(callback);
        path_listeners_[path].push_back(id);
        return id;
    }

    void remove_listener(listener_id id)
    {
        std::unique_lock lock(mutex_);
        listeners_.erase(id);
        for (auto &ids : path_listeners_ | std::views::values)
        {
            auto it = std::ranges::find(ids, id);
            if (it != ids.end())
            {
                ids.erase(it);
                break;
            }
        }
    }

    void notify_listeners(const std::string &path, const json &old_value, const json &new_value)
    {
        std::shared_lock lock(mutex_);
        const auto it = path_listeners_.find(path);
        if (it != path_listeners_.end())
        {
            for (auto id : it->second)
            {
                if (auto listener_it = listeners_.find(id); listener_it != listeners_.end())
                {
                    try
                    {
                        listener_it->second(old_value, new_value);
                    }
                    catch (...)
                    {
                    }
                }
            }
        }
    }
};

// 保存策略枚举
enum class SavePolicy
{
    AutoSave,   // 每次变更自动保存 (默认)
    ManualSave, // 手动保存
    BatchSave,  // 批量保存 (延迟保存)
    TimedSave   // 定时保存
};

// 配置存储类 - 支持JSON Pointer、路径策略和选择性值混淆
class config_store
{
    json data_;
    fs::path file_path_;
    Path path_policy_{Path::AutoDetect};
    std::string store_name_;
    mutable std::shared_mutex mutex_;
    std::unique_ptr<listener_manager> listeners_;
    SavePolicy save_policy_;
    std::atomic<bool> dirty_flag_{false};
    std::unique_ptr<std::thread> auto_save_thread_;
    std::chrono::milliseconds save_interval_{5000};
    std::atomic<bool> stop_flag_{false};
    std::condition_variable cv_;
    std::mutex cv_mutex_;

    // 混淆信息映射：键 -> 混淆策略
    std::unordered_map<std::string, Obfuscate> obfuscate_map_;
    static constexpr const char *OBFUSCATE_META_KEY = "__obfuscate_meta__";

    // 判断是否为JSON Pointer语法
    static bool is_json_pointer(const std::string &key)
    {
        return !key.empty() && key[0] == '/';
    }

    // 使用JSON Pointer获取值
    json get_value_by_pointer(const std::string &pointer) const
    {
        try
        {
            return data_.at(nlohmann::json::json_pointer(pointer));
        }
        catch (...)
        {
            return json{};
        }
    }

    // 使用JSON Pointer设置值
    void set_value_by_pointer(const std::string &pointer, const json &value)
    {
        try
        {
            data_[nlohmann::json::json_pointer(pointer)] = value;
        }
        catch (const std::exception &e)
        {
            throw config_exception("JSON Pointer error: " + std::string(e.what()));
        }
    }

    // 检查JSON Pointer路径是否存在
    bool contains_pointer(const std::string &pointer) const
    {
        try
        {
            data_.at(nlohmann::json::json_pointer(pointer));
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    // 混淆JSON数据（仅混淆指定的字段）
    json obfuscate_json_for_save(const json &data) const
    {
        json result = data;

        // 添加混淆元数据
        if (!obfuscate_map_.empty())
        {
            json meta;
            for (const auto &[key, policy] : obfuscate_map_)
            {
                meta[key] = static_cast<int>(policy);
            }
            result[OBFUSCATE_META_KEY] = meta;
        }

        // 混淆指定的字段
        for (const auto &[key, policy] : obfuscate_map_)
        {
            if (policy != Obfuscate::None)
            {
                if (is_json_pointer(key))
                {
                    auto value = get_value_by_pointer_from_json(result, key);
                    if (!value.is_null() && value.is_string())
                    {
                        set_value_by_pointer_in_json(result, key,
                                                     obfuscation_utils::obfuscate(value.get<std::string>(), policy));
                    }
                }
                else
                {
                    if (result.contains(key) && result[key].is_string())
                    {
                        result[key] = obfuscation_utils::obfuscate(result[key].get<std::string>(), policy);
                    }
                }
            }
        }

        return result;
    }

    // 解混淆JSON数据（根据元数据）
    json deobfuscate_json_from_load(const json &data) const
    {
        json result = data;

        // 读取混淆元数据
        std::unordered_map<std::string, Obfuscate> load_obfuscate_map;
        if (result.contains(OBFUSCATE_META_KEY))
        {
            auto meta = result[OBFUSCATE_META_KEY];
            for (const auto &[key, value] : meta.items())
            {
                if (value.is_number_integer())
                {
                    load_obfuscate_map[key] = static_cast<Obfuscate>(value.get<int>());
                }
            }
            result.erase(OBFUSCATE_META_KEY); // 移除元数据
        }

        // 解混淆指定的字段
        for (const auto &[key, policy] : load_obfuscate_map)
        {
            if (policy != Obfuscate::None)
            {
                if (is_json_pointer(key))
                {
                    auto value = get_value_by_pointer_from_json(result, key);
                    if (!value.is_null() && value.is_string())
                    {
                        try
                        {
                            set_value_by_pointer_in_json(
                                result, key, obfuscation_utils::deobfuscate(value.get<std::string>(), policy));
                        }
                        catch (...)
                        {
                            // 解混淆失败，保持原值
                        }
                    }
                }
                else
                {
                    if (result.contains(key) && result[key].is_string())
                    {
                        try
                        {
                            result[key] = obfuscation_utils::deobfuscate(result[key].get<std::string>(), policy);
                        }
                        catch (...)
                        {
                            // 解混淆失败，保持原值
                        }
                    }
                }
            }
        }

        // 更新当前实例的混淆映射
        const_cast<config_store *>(this)->obfuscate_map_ = load_obfuscate_map;

        return result;
    }

    // 从JSON中使用JSON Pointer获取值
    static json get_value_by_pointer_from_json(const json &data, const std::string &pointer)
    {
        try
        {
            return data.at(nlohmann::json::json_pointer(pointer));
        }
        catch (...)
        {
            return json{};
        }
    }

    // 在JSON中使用JSON Pointer设置值
    static void set_value_by_pointer_in_json(json &data, const std::string &pointer, const json &value)
    {
        try
        {
            data[nlohmann::json::json_pointer(pointer)] = value;
        }
        catch (...)
        {
            // 设置失败，忽略
        }
    }

    void load_from_file()
    {
        if (!fs::exists(file_path_))
        {
            data_ = json::object();
            return;
        }

        try
        {
            std::ifstream file(file_path_);
            if (!file.is_open())
            {
                throw config_exception("Failed to open config file: " + file_path_.string());
            }

            json file_data;
            file >> file_data;

            // 解混淆数据
            data_ = deobfuscate_json_from_load(file_data);
        }
        catch (const json::exception &e)
        {
            throw config_exception("JSON parse error: " + std::string(e.what()));
        }
        catch (const std::exception &e)
        {
            throw config_exception("File read error: " + std::string(e.what()));
        }
    }

    void save_to_file_internal()
    {
        try
        {
            fs::create_directories(file_path_.parent_path());

            std::ofstream file(file_path_);
            if (!file.is_open())
            {
                throw config_exception("Failed to write config file: " + file_path_.string());
            }

            // 混淆指定字段后保存
            const json obfuscated_data = obfuscate_json_for_save(data_);
            file << obfuscated_data.dump(4);
            file.flush();
            dirty_flag_ = false;
        }
        catch (const std::exception &e)
        {
            throw config_exception("Failed to save config file: " + std::string(e.what()));
        }
    }

    void trigger_save()
    {
        switch (save_policy_)
        {
        case SavePolicy::AutoSave:
            save_to_file_internal();
            break;
        case SavePolicy::BatchSave:
        case SavePolicy::TimedSave:
            dirty_flag_ = true;
            break;
        case SavePolicy::ManualSave:
            dirty_flag_ = true;
            break;
        }
    }

    void start_auto_save_thread()
    {
        if (save_policy_ == SavePolicy::TimedSave && !auto_save_thread_)
        {
            auto_save_thread_ = std::make_unique<std::thread>([this]() {
                for (;;)
                {
                    std::unique_lock<std::mutex> lk(cv_mutex_);
                    cv_.wait_for(lk, save_interval_, [this] { return stop_flag_.load() || dirty_flag_.load(); });
                    if (stop_flag_.load())
                        break;
                    if (save_policy_ == SavePolicy::TimedSave && dirty_flag_.load())
                    {
                        std::shared_lock lock(mutex_);
                        save_to_file_internal();
                    }
                }
            });
        }
    }

    void stop_auto_save_thread()
    {
        if (auto_save_thread_ && auto_save_thread_->joinable())
        {
            auto_save_thread_->join();
            auto_save_thread_.reset();
        }
    }

  public:
    explicit config_store(const std::string &name, SavePolicy policy = SavePolicy::AutoSave,
                          Path path_pol = Path::AutoDetect)
        : path_policy_(path_pol), store_name_(name), listeners_(std::make_unique<listener_manager>()),
          save_policy_(policy)
    {
        if (path_policy_ == Path::AutoDetect)
        {
            const auto appdata = path_manager::resolve_config_file(name, path_manager::get_appdata_directory());
            const auto current = path_manager::resolve_config_file(name, path_manager::get_current_dir_config());
            if (fs::exists(appdata))
                file_path_ = appdata;
            else if (fs::exists(current))
                file_path_ = current;
            else
                file_path_ = path_manager::get_config_path(name, path_policy_);
        }
        else if (path_policy_ == Path::CurrentDir)
        {
            file_path_ = path_manager::resolve_config_file(name, path_manager::get_current_dir_config());
        }
        else
        {
            file_path_ = path_manager::resolve_config_file(name, path_manager::get_appdata_directory());
        }

        load_from_file();
        start_auto_save_thread();
    }

    explicit config_store(const fs::path &path, SavePolicy policy = SavePolicy::AutoSave)
        : file_path_(path), path_policy_(Path::CurrentDir), store_name_(path.stem().string()),
          listeners_(std::make_unique<listener_manager>()), save_policy_(policy)
    {
        load_from_file();
        start_auto_save_thread();
    }

    ~config_store()
    {
        stop_flag_.store(true);
        cv_.notify_all();
        stop_auto_save_thread();
        if (dirty_flag_.load())
        {
            try
            {
                save_to_file_internal();
            }
            catch (...)
            {
            }
        }
    }

    // 基本配置方法 - 支持JSON Pointer
    template <typename T> T get(const std::string &key) const
    {
        std::shared_lock lock(mutex_);

        if (is_json_pointer(key))
        {
            auto value = get_value_by_pointer(key);
            if (value.is_null())
            {
                throw config_exception("JSON Pointer path not found: " + key);
            }
            return value.get<T>();
        }
        if (!data_.contains(key))
        {
            throw config_exception("Key not found: " + key);
        }
        return data_.at(key).get<T>();
    }

    template <typename T> T get_or(const std::string &key, const T &default_value) const
    {
        std::shared_lock lock(mutex_);

        if (is_json_pointer(key))
        {
            auto value = get_value_by_pointer(key);
            if (value.is_null())
            {
                return default_value;
            }
            try
            {
                return value.get<T>();
            }
            catch (...)
            {
                return default_value;
            }
        }
        return data_.value(key, default_value);
    }

    template <typename T> void set(const std::string &key, const T &value)
    {
        json old_value;
        {
            std::unique_lock lock(mutex_);

            if (is_json_pointer(key))
            {
                old_value = get_value_by_pointer(key);
                set_value_by_pointer(key, json(value));
            }
            else
            {
                old_value  = data_.contains(key) ? data_[key] : json{};
                data_[key] = value;
            }
        }

        listeners_->notify_listeners(key, old_value, json(value));
        trigger_save();
    }

    // 带混淆策略的set方法重载
    template <typename T> void set(const std::string &key, const T &value, Obfuscate obf_policy)
    {
        json old_value;
        {
            std::unique_lock lock(mutex_);

            if (is_json_pointer(key))
            {
                old_value = get_value_by_pointer(key);
                set_value_by_pointer(key, json(value));
            }
            else
            {
                old_value  = data_.contains(key) ? data_[key] : json{};
                data_[key] = value;
            }

            // 设置混淆策略
            if (obf_policy != Obfuscate::None)
            {
                obfuscate_map_[key] = obf_policy;
            }
            else
            {
                obfuscate_map_.erase(key);
            }
        }

        listeners_->notify_listeners(key, old_value, json(value));
        trigger_save();
    }

    // 专门的混淆设置方法
    template <typename T>
    void set_obfuscated(const std::string &key, const T &value, Obfuscate obf_policy = Obfuscate::Combined)
    {
        set(key, value, obf_policy);
    }

    // 监听器
    listener_id connect(const std::string &path, listener_callback callback) const
    {
        return listeners_->add_listener(path, std::move(callback));
    }

    void disconnect(listener_id id) const
    {
        listeners_->remove_listener(id);
    }

    // 实用方法 - 支持JSON Pointer
    bool contains(const std::string &key) const
    {
        std::shared_lock lock(mutex_);

        if (is_json_pointer(key))
        {
            return contains_pointer(key);
        }
        return data_.contains(key);
    }

    void remove(const std::string &key)
    {
        json old_value;
        {
            std::unique_lock lock(mutex_);

            if (is_json_pointer(key))
            {
                if (contains_pointer(key))
                {
                    old_value = get_value_by_pointer(key);
                    // JSON Pointer删除需要特殊处理
                    try
                    {
                        const auto ptr        = nlohmann::json::json_pointer(key);
                        const auto parent_ptr = ptr.parent_pointer();
                        const auto last_key   = ptr.back();

                        if (parent_ptr.empty())
                        {
                            data_.erase(last_key);
                        }
                        else
                        {
                            auto &parent = data_.at(parent_ptr);
                            if (parent.is_object())
                            {
                                parent.erase(last_key);
                            }
                            else if (parent.is_array())
                            {
                                try
                                {
                                    const size_t idx = static_cast<size_t>(std::stoll(last_key));
                                    if (idx < parent.size())
                                    {
                                        parent.erase(parent.begin() + static_cast<std::ptrdiff_t>(idx));
                                    }
                                }
                                catch (...)
                                {
                                }
                            }
                        }
                    }
                    catch (...)
                    {
                        return;
                    }
                }
                else
                {
                    return;
                }
            }
            else
            {
                if (data_.contains(key))
                {
                    old_value = data_[key];
                    data_.erase(key);
                }
                else
                {
                    return;
                }
            }

            // 同时移除混淆映射
            obfuscate_map_.erase(key);
        }

        listeners_->notify_listeners(key, old_value, json{});
        trigger_save();
    }

    void save()
    {
        save_to_file_internal();
    }

    void set_save_interval(std::chrono::milliseconds interval)
    {
        save_interval_ = interval;
        cv_.notify_all();
    }

    void reload()
    {
        std::unique_lock lock(mutex_);
        if (path_policy_ == Path::AutoDetect)
        {
            const auto appdata = path_manager::resolve_config_file(store_name_, path_manager::get_appdata_directory());
            const auto current = path_manager::resolve_config_file(store_name_, path_manager::get_current_dir_config());
            if (fs::exists(appdata))
                file_path_ = appdata;
            else if (fs::exists(current))
                file_path_ = current;
        }
        else if (path_policy_ == Path::CurrentDir)
        {
            file_path_ = path_manager::resolve_config_file(store_name_, path_manager::get_current_dir_config());
        }
        else
        {
            file_path_ = path_manager::resolve_config_file(store_name_, path_manager::get_appdata_directory());
        }
        load_from_file();
    }

    // 字符串转换
    std::string dump(int indent = 4) const
    {
        std::shared_lock lock(mutex_);
        return data_.dump(indent);
    }

    std::string to_string() const
    {
        return dump();
    }

    size_t size() const
    {
        std::shared_lock lock(mutex_);
        return data_.size();
    }

    bool empty() const
    {
        std::shared_lock lock(mutex_);
        return data_.empty();
    }

    bool is_dirty() const
    {
        return dirty_flag_.load();
    }

    // 获取当前配置文件路径
    fs::path get_file_path() const
    {
        return file_path_;
    }

    // 获取混淆策略
    Obfuscate get_obfuscate_policy(const std::string &key) const
    {
        std::shared_lock lock(mutex_);
        const auto it = obfuscate_map_.find(key);
        return (it != obfuscate_map_.end()) ? it->second : Obfuscate::None;
    }

    // 设置字段的混淆策略（不改变值）
    void set_obfuscate_policy(const std::string &key, Obfuscate policy)
    {
        std::unique_lock lock(mutex_);
        if (policy != Obfuscate::None)
        {
            obfuscate_map_[key] = policy;
        }
        else
        {
            obfuscate_map_.erase(key);
        }
        dirty_flag_ = true;
    }

    // 检查字段是否被混淆
    bool is_obfuscated(const std::string &key) const
    {
        return get_obfuscate_policy(key) != Obfuscate::None;
    }

    // 获取所有混淆字段
    std::vector<std::string> get_obfuscated_keys() const
    {
        std::shared_lock lock(mutex_);
        std::vector<std::string> keys;
        for (const auto &key : obfuscate_map_ | std::views::keys)
        {
            keys.push_back(key);
        }
        return keys;
    }

    // 流输出
    friend std::ostream &operator<<(std::ostream &os, const config_store &store)
    {
        os << store.dump(4);
        return os;
    }

    // JSON 访问
    json get_json() const
    {
        std::shared_lock lock(mutex_);
        return data_;
    }
};

// 全局注册表
class registry
{
    inline static std::unordered_map<std::string, std::shared_ptr<config_store>> stores_;
    inline static std::shared_mutex mutex_;

  public:
    static std::shared_ptr<config_store> get_store(const std::string &name, SavePolicy policy = SavePolicy::AutoSave,
                                                   Path path_pol = Path::AutoDetect)
    {
        std::shared_lock lock(mutex_);
        auto it = stores_.find(name);
        if (it != stores_.end())
        {
            return it->second;
        }

        lock.unlock();
        std::unique_lock ulock(mutex_);

        it = stores_.find(name);
        if (it != stores_.end())
        {
            return it->second;
        }

        auto store    = std::make_shared<config_store>(name, policy, path_pol);
        stores_[name] = store;
        return store;
    }

    static void remove_store(const std::string &name)
    {
        std::unique_lock lock(mutex_);
        stores_.erase(name);
    }

    static std::vector<std::string> list_stores()
    {
        std::shared_lock lock(mutex_);
        std::vector<std::string> result;
        for (const auto &name : stores_ | std::views::keys)
        {
            result.push_back(name);
        }
        return result;
    }
};

// 便捷函数
inline std::shared_ptr<config_store> get_store(const std::string &name, SavePolicy policy = SavePolicy::AutoSave,
                                               Path path_pol = Path::AutoDetect)
{
    return registry::get_store(name, policy, path_pol);
}

// 全局默认配置
class config
{
    static std::shared_ptr<config_store> get_default()
    {
        static auto default_store = registry::get_store("default");
        return default_store;
    }

  public:
    template <typename T> static T get(const std::string &key)
    {
        return get_default()->get<T>(key);
    }

    template <typename T> static T get_or(const std::string &key, const T &default_value)
    {
        return get_default()->get_or(key, default_value);
    }

    template <typename T> static void set(const std::string &key, const T &value)
    {
        get_default()->set(key, value);
    }

    template <typename T> static void set(const std::string &key, const T &value, Obfuscate obf_policy)
    {
        get_default()->set(key, value, obf_policy);
    }

    template <typename T>
    static void set_obfuscated(const std::string &key, const T &value, Obfuscate obf_policy = Obfuscate::Combined)
    {
        get_default()->set_obfuscated(key, value, obf_policy);
    }

    static std::string dump(int indent = 4)
    {
        return get_default()->dump(indent);
    }

    static void save()
    {
        get_default()->save();
    }

    static bool contains(const std::string &key)
    {
        return get_default()->contains(key);
    }

    static void remove(const std::string &key)
    {
        get_default()->remove(key);
    }
};

// 全局便捷函数
template <typename T> T get(const std::string &key)
{
    return config::get<T>(key);
}

template <typename T> T get_or(const std::string &key, const T &default_value)
{
    return config::get_or(key, default_value);
}

template <typename T> void set(const std::string &key, const T &value)
{
    config::set(key, value);
}

template <typename T> void set(const std::string &key, const T &value, Obfuscate obf_policy)
{
    config::set(key, value, obf_policy);
}

template <typename T>
void set_obfuscated(const std::string &key, const T &value, Obfuscate obf_policy = Obfuscate::Combined)
{
    config::set_obfuscated(key, value, obf_policy);
}

inline std::string dump(int indent = 4)
{
    return config::dump(indent);
}
} // namespace config