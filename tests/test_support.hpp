#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <config/config.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <gtest/gtest.h>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

namespace config_test
{
using json = nlohmann::json;
using namespace std::chrono_literals;

inline void expect_error(config::ErrorCode code, const std::function<void()> &operation)
{
    try
    {
        operation();
        FAIL() << "Expected config::Error";
    }
    catch (const config::Error &error)
    {
        EXPECT_EQ(error.code(), code) << error.what();
    }
}

class Gate
{
    std::mutex mutex_;
    std::condition_variable changed_;
    bool entered_  = false;
    bool released_ = false;

  public:
    void block()
    {
        std::unique_lock lock(mutex_);
        entered_ = true;
        changed_.notify_all();
        if (!changed_.wait_for(lock, 5s, [&] { return released_; }))
            throw std::runtime_error("Gate release timed out");
    }
    bool entered()
    {
        std::unique_lock lock(mutex_);
        return changed_.wait_for(lock, 5s, [&] { return entered_; });
    }
    void release()
    {
        std::lock_guard lock(mutex_);
        released_ = true;
        changed_.notify_all();
    }
};

template <class Predicate> bool eventually(Predicate predicate)
{
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (!predicate())
    {
        if (std::chrono::steady_clock::now() > deadline)
            return false;
        std::this_thread::sleep_for(5ms);
    }
    return true;
}

class ControlledBackend : public config::Backend
{
    config::detail::FileBackend files_;

  public:
    std::atomic<bool> fail{false};
    std::atomic<int> writes{0};
    std::function<void(int)> before_write;
    std::optional<std::string> read(const std::filesystem::path &file) override
    {
        return files_.read(file);
    }
    void write(const std::filesystem::path &file, std::string_view text) override
    {
        const auto number = ++writes;
        if (before_write)
            before_write(number);
        if (fail)
            throw config::Error(config::ErrorCode::write, "Injected write failure", file);
        files_.write(file, text);
    }
};
class ScopedEnv
{
    std::string name_;
    std::optional<std::string> previous_;

    int assign(const std::optional<std::string> &value) const
    {
#if defined(_WIN32)
        return _putenv_s(name_.c_str(), value ? value->c_str() : "");
#else
        return value ? setenv(name_.c_str(), value->c_str(), 1) : unsetenv(name_.c_str());
#endif
    }

  public:
    ScopedEnv(std::string name, std::optional<std::string> value) : name_(std::move(name))
    {
#if defined(_WIN32)
        char *previous     = nullptr;
        std::size_t length = 0;
        if (_dupenv_s(&previous, &length, name_.c_str()) != 0)
            throw std::runtime_error("Cannot read test environment variable");
        if (previous)
        {
            previous_ = previous;
            std::free(previous);
        }
#else
        if (const char *previous = std::getenv(name_.c_str()))
            previous_ = previous;
#endif
        if (assign(value) != 0)
            throw std::runtime_error("Cannot set test environment variable");
    }
    ~ScopedEnv()
    {
        assign(previous_);
    }
    ScopedEnv(const ScopedEnv &)            = delete;
    ScopedEnv &operator=(const ScopedEnv &) = delete;
};

class TempDirectory
{
    std::filesystem::path root_;
    inline static std::atomic<unsigned long long> sequence_{0};

  public:
    TempDirectory()
    {
        const auto base = std::filesystem::canonical(std::filesystem::temp_directory_path());
        do
        {
            root_ = base / ("config-regression-" +
                            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
                            std::to_string(sequence_.fetch_add(1)));
        } while (!std::filesystem::create_directory(root_));
    }
    ~TempDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(root_, error);
    }
    TempDirectory(const TempDirectory &)            = delete;
    TempDirectory &operator=(const TempDirectory &) = delete;
    std::filesystem::path file(const std::string &name = "config.json") const
    {
        return root_ / name;
    }
    const std::filesystem::path &path() const
    {
        return root_;
    }
};

inline std::string read(const std::filesystem::path &path)
{
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

inline void write(const std::filesystem::path &path, const std::string &contents)
{
    std::ofstream stream(path, std::ios::binary);
    stream.exceptions(std::ios::failbit | std::ios::badbit);
    stream << contents;
    stream.close();
}
} // namespace config_test
