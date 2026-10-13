#pragma once

#include <config/detail/key_path.hpp>
#include <config/detail/platform.hpp>
#include <cstdlib>
#include <cwchar>

#if defined(__APPLE__)
#include <crt_externs.h>
#elif !defined(_WIN32)
extern "C" char **environ;
#endif

namespace config::detail
{
inline std::map<std::string, std::string> environment()
{
    std::map<std::string, std::string> result;
#if defined(_WIN32)
    struct Block
    {
        wchar_t *value = GetEnvironmentStringsW();
        ~Block()
        {
            if (value)
                FreeEnvironmentStringsW(value);
        }
    } block;
    if (!block.value)
        throw Error(ErrorCode::read, "Cannot read process environment");
    const auto utf8 = [](const std::wstring &text) {
        const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.c_str(), static_cast<int>(text.size()),
                                             nullptr, 0, nullptr, nullptr);
        if (!size && !text.empty())
            throw Error(ErrorCode::type, "Invalid environment text");
        std::string out(static_cast<std::size_t>(size), '\0');
        if (size)
            WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.c_str(), static_cast<int>(text.size()), out.data(),
                                size, nullptr, nullptr);
        return out;
    };
    for (auto entry = block.value; *entry; entry += std::wcslen(entry) + 1)
    {
        const std::wstring text(entry);
        const auto pos = text.find(L'=');
        if (pos != std::wstring::npos && pos != 0)
            result[utf8(text.substr(0, pos))] = utf8(text.substr(pos + 1));
    }
#else
#if defined(__APPLE__)
    auto entries = *_NSGetEnviron();
#else
    auto entries = environ;
#endif
    if (entries)
        for (auto entry = entries; *entry; ++entry)
        {
            const std::string text(*entry);
            const auto pos = text.find('=');
            if (pos != std::string::npos)
                result[text.substr(0, pos)] = text.substr(pos + 1);
        }
#endif
    return result;
}

inline json environment_layer(const StoreOptions &options)
{
    json result = json::object();
    if (options.env_prefix.empty() && options.env_bindings.empty())
        return result;
    const auto variables = environment();
    const auto assign    = [&](const std::string &key, const std::string &value) {
        auto parsed = json::parse(value, nullptr, false);
        try
        {
            result[pointer(key)] = parsed.is_discarded() ? json(value) : std::move(parsed);
        }
        catch (const json::exception &e)
        {
            throw Error(ErrorCode::path, e.what());
        }
    };
    if (!options.env_prefix.empty())
        for (const auto &[name, value] : variables)
        {
            if (!name.starts_with(options.env_prefix) || name.size() == options.env_prefix.size())
                continue;
            std::string key = "/";
            for (const char c : name.substr(options.env_prefix.size()))
            {
                if (c == '_')
                    key += '/';
                else if (c == '~')
                    key += "~0";
                else if (c == '/')
                    key += "~1";
                else
                    key += c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
            }
            assign(key, value);
        }
    for (const auto &[key, name] : options.env_bindings)
    {
        const auto found = variables.find(name);
        if (found != variables.end())
            assign(key, found->second);
    }
    return result;
}
} // namespace config::detail
