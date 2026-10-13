#pragma once

#include <config/detail/encoding.hpp>
#include <config/detail/key_path.hpp>
#include <optional>

namespace config::detail
{
struct Document
{
    json values = json::object();
    Encodings encodings;
};

inline Document decode(const std::optional<std::string> &text, const std::filesystem::path &file, bool legacy = false)
{
    if (!text)
        return {};
    try
    {
        auto root = json::parse(*text);
        require_object(root);
        Document result;
        json rules = json::object();
        if (root.contains("$config") && root["$config"] == "config/v1")
        {
            if (root.size() != 3 || !root.contains("values") || !root.contains("encoding"))
                throw Error(ErrorCode::format, "Invalid config/v1 envelope", file);
            result.values = root.at("values");
            rules         = root.at("encoding");
        }
        else
        {
            if (root.contains("$config") && root["$config"].is_string() &&
                root["$config"].get_ref<const std::string &>().starts_with("config/"))
                throw Error(ErrorCode::format, "Unsupported configuration format version", file);
            result.values = std::move(root);
            if (legacy && result.values.contains("__obfuscate_meta__"))
            {
                rules = result.values.at("__obfuscate_meta__");
                result.values.erase("__obfuscate_meta__");
            }
        }
        require_object(result.values);
        require_object(rules);
        for (const auto &[key, rule] : rules.items())
        {
            const auto canonical = legacy && !key.empty() && key.front() != '/' ? "/" + key : key;
            const auto p         = pointer(canonical);
            if (!rule.is_number_integer())
                throw Error(ErrorCode::format, "Invalid encoding rule", file);
            const auto value = rule.get<int>();
            if (value < 0 || value > static_cast<int>(Encoding::combined) || !result.values.contains(p) ||
                !result.values.at(p).is_string())
                throw Error(ErrorCode::format, "Encoding rule must address a string and use a supported encoding",
                            file);
            const auto encoding = static_cast<Encoding>(value);
            auto &encoded       = result.values.at(p).get_ref<std::string &>();
            auto decoded        = EncodingEngine::decode(encoded, encoding);
            if (encoding == Encoding::base64 || encoding == Encoding::combined)
            {
                if (EncodingEngine::encode(decoded, encoding) != encoded)
                    throw Error(ErrorCode::format, "Invalid Base64 data", file);
            }
            if (encoding == Encoding::hex &&
                (encoded.size() % 2 || encoded.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos))
                throw Error(ErrorCode::format, "Invalid hexadecimal data", file);
            encoded = std::move(decoded);
            if (encoding != Encoding::none)
                result.encodings.emplace(canonical, encoding);
        }
        return result;
    }
    catch (const Error &)
    {
        throw;
    }
    catch (const json::exception &e)
    {
        throw Error(ErrorCode::parse, e.what(), file);
    }
}

inline std::string encode(const Document &document, Format format, const std::filesystem::path &file)
{
    try
    {
        json values = document.values;
        json rules  = json::object();
        for (const auto &[key, encoding] : document.encodings)
        {
            const auto p = pointer(key);
            if (!values.contains(p) || !values.at(p).is_string())
                throw Error(ErrorCode::format, "Encoding rule no longer addresses a string", file);
            values[p]  = EncodingEngine::encode(values.at(p).get_ref<const std::string &>(), encoding);
            rules[key] = static_cast<int>(encoding);
        }
        return json{{"$config", "config/v1"}, {"values", std::move(values)}, {"encoding", std::move(rules)}}.dump(
            format == Format::pretty ? 4 : -1);
    }
    catch (const json::exception &e)
    {
        throw Error(ErrorCode::format, e.what(), file);
    }
}
} // namespace config::detail
