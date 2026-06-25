#pragma once

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <ranges>
#include <shared_mutex>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

/**
 * @brief Macro alias for nlohmann::json's non-intrusive struct serialization.
 * Use this to define JSON binding for a struct outside of the struct definition.
 */
#define CONFIG_STRUCT NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE

/**
 * @brief Like CONFIG_STRUCT, but missing JSON fields fall back to the struct's
 *        default-constructed values instead of throwing.
 */
#define CONFIG_STRUCT_WITH_DEFAULT NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT

#if defined(_WIN32)
#include <shlobj.h>
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#elif defined(__linux__)
#include <limits.h>
#include <unistd.h>
#endif

#include <config/detail/obfuscation.hpp>
#include <config/detail/path_resolver.hpp>
#include <config/detail/types.hpp>
#include <config/store.hpp>
