#pragma once

#include "bdmicros.h"

#define PLUGIN_API_VERSION "1.0.0"

typedef enum {
    PLUGIN_STATE_UNLOADED = 0,
    PLUGIN_STATE_LOADED,
    PLUGIN_STATE_ENABLED,
    PLUGIN_STATE_DISABLED,
    PLUGIN_STATE_ERROR
} PluginState;

typedef struct {
    char name[64];
    char version[16];
    char author[64];
    char description[256];
    char main_dll[128];
    char api_version[16];
    int hook_count;
} PluginManifest;

typedef struct {
    int hook_id;
    BDM_HOOK_TYPE type;
    void* callback;
    void* user_data;
    int priority;
} HookInfo;

typedef void (*HookCallback)(void* hwnd, UINT msg, WPARAM wParam, LPARAM lParam, void* user_data);

typedef struct {
    void* env;
    HWND target_hwnd;
    HWND core_hwnd;
    DWORD target_pid;
    const wchar_t* plugin_dir;
    void* flutter_engine;
} PluginEnv;

typedef bool (*Plugin_OnLoad)(PluginEnv* env);
typedef void (*Plugin_OnUnload)();
typedef void (*Plugin_OnEnable)();
typedef void (*Plugin_OnDisable)();
typedef const char* (*Plugin_GetName)();
typedef const char* (*Plugin_GetVersion)();

typedef struct {
    int api_version;

    Plugin_OnLoad on_load;
    Plugin_OnUnload on_unload;
    Plugin_OnEnable on_enable;
    Plugin_OnDisable on_disable;
    Plugin_GetName get_name;
    Plugin_GetVersion get_version;

    int (*register_hook)(HookInfo* info);
    int (*unregister_hook)(int hook_id);

    int (*call_flutter_method)(const char* method, const char* args, char** result);
    int (*listen_flutter_event)(const char* channel, void (*callback)(const char* event, void* data), void* user_data);

    void (*log)(int level, const char* format, ...);

    void* (*malloc)(size_t size);
    void (*free)(void* ptr);

    void* user_data;
} PluginInterface;

#define PLUGIN_INIT_EXPORT_NAME "Plugin_Initialize"
#define PLUGIN_GET_API_EXPORT_NAME "Plugin_GetAPI"

typedef PluginInterface* (*Plugin_GetAPIFn)(void* reserved);

typedef struct {
    int api_version;
    Plugin_GetAPIFn get_api;
} PluginExports;
