#pragma once

namespace config
{

/**
 * @brief Enum defining path types for configuration files.
 */
enum class Path
{
    Absolute, ///< Absolute file path.
    Relative, ///< Relative path to the current working directory.
    AppData   ///< System-specific application data directory (e.g., %APPDATA% on Windows).
};

/**
 * @brief Enum defining JSON output formats.
 */
enum class JsonFormat
{
    Pretty, ///< Indented output (4 spaces) for readability.
    Compact ///< Minified output without whitespace.
};

/**
 * @brief Enum defining available obfuscation methods.
 */
enum class Obfuscate
{
    None,    ///< No obfuscation (plaintext).
    Base64,  ///< Base64 encoding.
    Hex,     ///< Hexadecimal string encoding.
    ROT13,   ///< ROT13 substitution cipher.
    Reverse, ///< String reversal.
    Combined ///< Combined strategy (Base64 + Reverse).
};

/**
 * @brief Enum defining when configuration changes are saved to disk.
 */
enum class SaveStrategy
{
    Auto,  ///< Automatically save to disk after every 'set' operation.
    Manual ///< Only save to disk when 'save()' is explicitly called.
};

/**
 * @brief Enum defining behavior when a key is missing during retrieval.
 */
enum class GetStrategy
{
    DefaultValue,  ///< Return a type-specific default value if key is missing.
    ThrowException ///< Throw a std::runtime_error if key is missing.
};

} // namespace config
