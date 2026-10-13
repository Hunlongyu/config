#pragma once

#include <algorithm>
#include <charconv>
#include <config/error.hpp>
#include <config/options.hpp>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

namespace config::detail
{
using json      = nlohmann::json;
using Encodings = std::map<std::string, Encoding>;

inline json::json_pointer pointer(std::string_view key)
{
    if (key.empty() || key.front() != '/')
        throw Error(ErrorCode::path, "Keys must be nonempty JSON Pointers beginning with '/'");
    try
    {
        return json::json_pointer(std::string(key));
    }
    catch (const json::exception &e)
    {
        throw Error(ErrorCode::path, e.what());
    }
}

inline bool subtree(std::string_view path, std::string_view root)
{
    return path == root || (path.size() > root.size() && path.starts_with(root) && path[root.size()] == '/');
}

inline void require_object(const json &value)
{
    if (!value.is_object())
        throw Error(ErrorCode::type, "Configuration root must be a JSON object");
}

inline void deep_merge(json &target, const json &overlay)
{
    for (const auto &[key, value] : overlay.items())
    {
        if (value.is_object() && target.contains(key) && target[key].is_object())
            deep_merge(target[key], value);
        else
            target[key] = value;
    }
}

inline void prune_encodings(Encodings &rules, const json &before, const json &after)
{
    std::erase_if(rules, [&](const auto &rule) {
        const auto p = pointer(rule.first);
        return !before.contains(p) || !after.contains(p) || before.at(p) != after.at(p);
    });
}

inline void erase_value(json &data, Encodings &rules, const json::json_pointer &p)
{
    if (!data.contains(p))
        return;
    auto &parent   = data.at(p.parent_pointer());
    const auto key = p.to_string();
    if (parent.is_array())
    {
        std::size_t index = 0;
        const auto token  = p.back();
        const auto result = std::from_chars(token.data(), token.data() + token.size(), index);
        if (result.ec != std::errc{} || result.ptr != token.data() + token.size())
            throw Error(ErrorCode::path, "Invalid array index");
        parent.erase(parent.begin() + static_cast<json::difference_type>(index));
        const auto prefix = p.parent_pointer().to_string() + "/";
        Encodings shifted;
        for (const auto &[path, encoding] : rules)
        {
            if (!path.starts_with(prefix))
            {
                shifted[path] = encoding;
                continue;
            }
            const auto end = path.find('/', prefix.size());
            const auto part =
                std::string_view(path).substr(prefix.size(), end == std::string::npos ? end : end - prefix.size());
            std::size_t old   = 0;
            const auto parsed = std::from_chars(part.data(), part.data() + part.size(), old);
            if (parsed.ec != std::errc{} || parsed.ptr != part.data() + part.size())
                continue;
            if (old == index)
                continue;
            shifted[prefix + std::to_string(old > index ? old - 1 : old) +
                    (end == std::string::npos ? "" : path.substr(end))] = encoding;
        }
        rules = std::move(shifted);
    }
    else
    {
        parent.erase(p.back());
        std::erase_if(rules, [&](const auto &rule) { return subtree(rule.first, key); });
    }
}
} // namespace config::detail
