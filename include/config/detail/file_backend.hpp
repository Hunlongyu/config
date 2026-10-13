#pragma once

#include <config/backend.hpp>
#include <config/detail/atomic_file.hpp>
#include <config/detail/platform.hpp>
#include <config/error.hpp>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <vector>

namespace config::detail
{
inline std::filesystem::path normalize_file(const std::filesystem::path &path)
{
    if (path.empty())
        throw Error(ErrorCode::path, "Configuration file path is empty");
    std::error_code error;
    auto absolute = std::filesystem::absolute(path, error);
    if (error)
        throw Error(ErrorCode::path, error.message(), path);
    auto canonical = std::filesystem::weakly_canonical(absolute, error);
    if (error)
        throw Error(ErrorCode::path, error.message(), path);
    return canonical;
}

inline bool same_file(const std::filesystem::path &a, const std::filesystem::path &b)
{
    if (a == b)
        return true;
#if defined(_WIN32)
    if (CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_EQUAL)
        return true;
#endif
    std::error_code error;
    return std::filesystem::equivalent(a, b, error) && !error;
}

// Track active owners weakly without retaining stores or caching expired instances.
inline std::shared_ptr<const std::filesystem::path> claim_file(const std::filesystem::path &path)
{
    static std::mutex mutex;
    static std::vector<std::weak_ptr<const std::filesystem::path>> owners;
    std::lock_guard lock(mutex);
    std::erase_if(owners, [](const auto &owner) { return owner.expired(); });
    for (const auto &owner : owners)
        if (auto file = owner.lock(); file && same_file(*file, path))
            throw Error(ErrorCode::in_use, "Configuration file already has an active Store", path);
    auto file = std::make_shared<const std::filesystem::path>(path);
    owners.push_back(file);
    return file;
}

class FileBackend final : public Backend
{
  public:
    std::optional<std::string> read(const std::filesystem::path &path) override
    {
        std::error_code error;
        const auto status = std::filesystem::status(path, error);
        if (status.type() == std::filesystem::file_type::not_found)
        {
            // Windows also reports a missing path when a parent is a regular file.
            for (auto parent = path.parent_path(); !parent.empty();)
            {
                std::error_code parent_error;
                const auto parent_status = std::filesystem::status(parent, parent_error);
                if (std::filesystem::exists(parent_status))
                {
                    if (!std::filesystem::is_directory(parent_status))
                        throw Error(ErrorCode::read, "Configuration parent is not a directory", path);
                    break;
                }
                if (parent_error && parent_error != std::errc::no_such_file_or_directory &&
                    parent_error != std::errc::not_a_directory)
                    throw Error(ErrorCode::read, parent_error.message(), path);
                const auto next = parent.parent_path();
                if (next == parent)
                    break;
                parent = next;
            }
            if (error && error != std::errc::no_such_file_or_directory && error != std::errc::not_a_directory)
                throw Error(ErrorCode::read, error.message(), path);
            return std::nullopt;
        }
        if (error || !std::filesystem::is_regular_file(status))
            throw Error(ErrorCode::read, error ? error.message() : "Not a regular file", path);
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
            throw Error(ErrorCode::read, "Cannot open configuration", path);
        std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
        if (stream.bad())
            throw Error(ErrorCode::read, "Cannot read configuration", path);
        return text;
    }
    void write(const std::filesystem::path &path, std::string_view text) override
    {
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        if (error)
            throw Error(ErrorCode::write, error.message(), path);
        AtomicFile file(path);
        if (!file.commit(path, text))
            throw Error(ErrorCode::write, "Cannot atomically replace configuration", path);
    }
};
} // namespace config::detail
