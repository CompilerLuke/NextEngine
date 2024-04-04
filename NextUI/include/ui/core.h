#pragma once

#if 0
#ifdef NE_PLATFORM_WINDOWS
    #ifdef NEXTUI_EXPORTS
    #define UI_API __declspec(dllexport)
    #else
    #define UI_API __declspec(dllimport)
    #endif
#else
    #define UI_API
#endif
#else
#define UI_API 
#endif
