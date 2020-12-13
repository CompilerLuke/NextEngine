#pragma once

#include "core/core.h"

#ifdef NEXTENGINE_EXPORTS
    #ifdef NE_PLATFORM_WINDOWS
        #define ENGINE_API __declspec(dllexport)
    #else
        #define ENGINE_API
    #endif
#else
    #ifdef NE_PLATFORM_WINDOWS
        #define ENGINE_API __declspec(dllimport)
    #else
        #define ENGINE_API
    #endif
#endif
