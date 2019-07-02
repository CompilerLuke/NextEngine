#pragma once

#ifdef NEXTENGINE_EXPORTS
#define ENGINE_API __declspec(dllexport)
#endif
#ifndef NEXTENGINE_EXPORTS
#define ENGINE_API __declspec(dllimport)
#endif