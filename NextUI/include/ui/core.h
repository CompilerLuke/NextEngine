#pragma once

#ifdef NE_PLATFORM_WINDOWS
#ifdef NEXTUI_EXPORTS
#define UI_API __declspec(dllexport)
#else
#define UI_API __declspec(dllimport)
#endif
#endif