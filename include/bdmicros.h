#pragma once

#include <windows.h>

#define BDM_VERSION "1.0.0"
#define BDM_API_VERSION "1.0.0"

#ifdef BDM_EXPORTS
#define BDM_API __declspec(dllexport)
#else
#define BDM_API __declspec(dllimport)
#endif

#define BDM_LOG_LEVEL_DEBUG 0
#define BDM_LOG_LEVEL_INFO  1
#define BDM_LOG_LEVEL_WARN  2
#define BDM_LOG_LEVEL_ERROR 3

#ifndef ARRAYSIZE
#define ARRAYSIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#define WM_BDM_INJECTED    (WM_USER + 100)
#define WM_BDM_PLUGIN_LOAD (WM_USER + 101)
#define WM_BDM_PLUGIN_UNLOAD (WM_USER + 102)

typedef enum {
    BDM_HOOK_WNDPROC = 0,
    BDM_HOOK_SETWINDOWLONGPTR,
    BDM_HOOK_WIN32_API,
    BDM_HOOK_SETWINEVENT,
    BDM_HOOK_FLUTTER_ENGINE
} BDM_HOOK_TYPE;

typedef enum {
    BDM_RESULT_SUCCESS = 0,
    BDM_RESULT_ERROR_GENERAL = -1,
    BDM_RESULT_ERROR_NOT_FOUND = -2,
    BDM_RESULT_ERROR_ALREADY_EXISTS = -3,
    BDM_RESULT_ERROR_INVALID_PARAM = -4,
    BDM_RESULT_ERROR_NO_MEMORY = -5,
    BDM_RESULT_ERROR_PERMISSION = -6,
    BDM_RESULT_ERROR_TIMEOUT = -7
} BDM_RESULT;

typedef struct {
    DWORD version;
    DWORD size;
    HWND target_hwnd;
    HWND core_hwnd;
    DWORD target_pid;
    const wchar_t* plugin_dir;
    void* flutter_engine;
} BDM_StartupInfo;

typedef void (*BDM_LogCallback)(int level, const char* message);

BDM_API void BDM_SetLogCallback(BDM_LogCallback callback);
BDM_API void BDM_Log(int level, const char* format, ...);

typedef BDM_RESULT (*BDM_InitializeFn)(BDM_StartupInfo* info);
typedef void (*BDM_UninitializeFn)();
typedef BDM_RESULT (*BDM_GetVersionFn)(char* version, int size);
