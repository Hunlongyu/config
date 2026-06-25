#pragma once

#include <algorithm>
#include <format>
#include <ranges>
#include <string>
#include <string_view>

#include <config/detail/types.hpp>

namespace config::detail
{

class ObfuscationEngine
{
    static constexpr char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    static constexpr bool is_base64(const unsigned char c)
    {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || (c == '+') || (c == '/');
    }

  public:
    static std::string base64_encode(std::string_view input)
    {
        std::string ret;
        ret.reserve(((input.length() + 2) / 3) * 4);
        int i = 0;
        unsigned char char_array_3[3];
        unsigned char char_array_4[4];
        auto it       = input.begin();
        size_t in_len = input.length();

        while (in_len--)
        {
            char_array_3[i++] = *(it++);
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

    static std::string base64_decode(std::string_view input)
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
                    char_array_4[i] = static_cast<unsigned char>(std::string_view(base64_chars).find(char_array_4[i]));

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
                char_array_4[j] = static_cast<unsigned char>(std::string_view(base64_chars).find(char_array_4[j]));

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (j = 0; (j < i - 1); j++)
                ret += char_array_3[j];
        }

        return ret;
    }

    static std::string hex_encode(std::string_view input)
    {
        std::string result;
        result.reserve(input.length() * 2);
        for (const unsigned char c : input)
        {
            std::format_to(std::back_inserter(result), "{:02x}", c);
        }
        return result;
    }

    static std::string hex_decode(std::string_view input)
    {
        std::string result;
        if (input.length() % 2 == 0)
        {
            for (size_t i = 0; i < input.length(); i += 2)
            {
                std::string byteString(input.substr(i, 2));
                const char byte = static_cast<char>(strtol(byteString.c_str(), nullptr, 16));
                result += byte;
            }
        }
        return result;
    }

    static std::string rot13(std::string_view input)
    {
        std::string result;
        result.reserve(input.size());
        auto transform_char = [](char c) -> char {
            if (c >= 'a' && c <= 'z')
                return (c - 'a' + 13) % 26 + 'a';
            if (c >= 'A' && c <= 'Z')
                return (c - 'A' + 13) % 26 + 'A';
            return c;
        };
        std::ranges::transform(input, std::back_inserter(result), transform_char);
        return result;
    }

    static std::string reverse(std::string_view input)
    {
        std::string result(input);
        std::ranges::reverse(result);
        return result;
    }

    static std::string encrypt(std::string_view input, const Obfuscate obf)
    {
        std::string result;
        switch (obf)
        {
        case Obfuscate::Base64:
            result = base64_encode(input);
            break;
        case Obfuscate::Hex:
            result = hex_encode(input);
            break;
        case Obfuscate::ROT13:
            result = rot13(input);
            break;
        case Obfuscate::Reverse:
            result = reverse(input);
            break;
        case Obfuscate::Combined:
            result = reverse(base64_encode(input));
            break;
        case Obfuscate::None:
        default:
            result = std::string(input);
            break;
        }
        return result;
    }

    static std::string decrypt(std::string_view input, const Obfuscate obf)
    {
        std::string result;
        switch (obf)
        {
        case Obfuscate::Base64:
            result = base64_decode(input);
            break;
        case Obfuscate::Hex:
            result = hex_decode(input);
            break;
        case Obfuscate::ROT13:
            result = rot13(input); // ROT13 is symmetric
            break;
        case Obfuscate::Reverse:
            result = reverse(input); // Reverse is symmetric
            break;
        case Obfuscate::Combined:
            result = base64_decode(reverse(input));
            break;
        case Obfuscate::None:
        default:
            result = std::string(input);
            break;
        }
        return result;
    }
};

} // namespace config::detail
