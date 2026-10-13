#pragma once

#if defined(_WIN32)
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#ifndef NOMINMAX
#define CONFIG_UNDEFINE_NOMINMAX
#define NOMINMAX
#endif
#include <shlobj.h>
#include <windows.h>
#ifdef CONFIG_UNDEFINE_NOMINMAX
#undef NOMINMAX
#undef CONFIG_UNDEFINE_NOMINMAX
#endif
#pragma pop_macro("max")
#pragma pop_macro("min")
#endif
