#pragma once

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>

namespace config::detail
{

// Replace from the same directory; on failure, remove only the file we created.
class AtomicFile
{
    std::filesystem::path temporary_;
    std::FILE *file_ = nullptr;
    bool owns_file_  = false;
    inline static std::atomic<unsigned long long> sequence_{0};

  public:
    explicit AtomicFile(const std::filesystem::path &target)
    {
        for (int attempt = 0; attempt < 100; ++attempt)
        {
            temporary_ = target;
            temporary_ += ".tmp." + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "." +
                          std::to_string(sequence_.fetch_add(1));
#if defined(_WIN32)
            const int error = _wfopen_s(&file_, temporary_.c_str(), L"wbx");
#else
            file_           = std::fopen(temporary_.c_str(), "wbx");
            const int error = file_ ? 0 : errno;
#endif
            if (file_)
            {
                owns_file_ = true;
                break;
            }
            if (error != EEXIST)
                break;
        }
    }

    AtomicFile(const AtomicFile &)            = delete;
    AtomicFile &operator=(const AtomicFile &) = delete;

    ~AtomicFile()
    {
        if (file_)
            std::fclose(file_);
        if (owns_file_)
        {
            std::error_code error;
            std::filesystem::remove(temporary_, error);
        }
    }

    bool commit(const std::filesystem::path &target, std::string_view contents)
    {
        if (!file_)
            return false;
        const bool written = std::fwrite(contents.data(), 1, contents.size(), file_) == contents.size();
        const bool flushed = std::fflush(file_) == 0;
        const bool closed  = std::fclose(file_) == 0;
        file_              = nullptr;
        if (!written || !flushed || !closed)
            return false;
        std::error_code error;
        std::filesystem::rename(temporary_, target, error);
        if (!error)
            owns_file_ = false;
        return !error;
    }
};

} // namespace config::detail
