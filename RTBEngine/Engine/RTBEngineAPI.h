#pragma once

#ifdef RTB_ENGINE_EXPORTS
    #define RTB_API __declspec(dllexport)
#else
    #define RTB_API __declspec(dllimport)
#endif
