#include "core.h"

static BDM_LogCallback g_logCallback = NULL;
static HWND g_targetHwnd = NULL;
static wchar_t g_pluginDir[MAX_PATH] = L"./plugins";
static BOOL g_initialized = FALSE;

static void DefaultLogCallback(int level, const char* message) {
    const char* level_str[] = { "DEBUG", "INFO", "WARN", "ERROR" };
    SYSTEMTIME st;
    GetLocalTime(&st);
    printf("[%02d:%02d:%02d] [%s] %s\n", st.wHour, st.wMinute, st.wSecond, level_str[level], message);
}

extern "C" {

void BDM_SetLogCallback(BDM_LogCallback callback) {
    g_logCallback = callback;
}

void BDM_Log(int level, const char* format, ...) {
    char buffer[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (g_logCallback) {
        g_logCallback(level, buffer);
    } else {
        DefaultLogCallback(level, buffer);
    }
}

BDM_API BDM_RESULT Core_Initialize(BDM_StartupInfo* info) {
    if (g_initialized) {
        return BDM_RESULT_SUCCESS;
    }

    BDM_Log(BDM_LOG_LEVEL_INFO, "BetterBDM Core v%s initializing...", CORE_VERSION);

    if (info) {
        g_targetHwnd = info->target_hwnd;
        if (info->plugin_dir) {
            wcsncpy(g_pluginDir, info->plugin_dir, MAX_PATH - 1);
            g_pluginDir[MAX_PATH - 1] = L'\0';
        }
    }

    if (!g_targetHwnd) {
        g_targetHwnd = FindWindowW(L"FlutterView", NULL);
        if (!g_targetHwnd) {
            g_targetHwnd = FindWindowW(NULL, L"波点音乐");
        }
    }

    BDM_Log(BDM_LOG_LEVEL_INFO, "Core: Target HWND = %p", g_targetHwnd);

    BDM_RESULT result = FlutterBridge::Instance().Initialize(g_targetHwnd, info ? info->flutter_engine : NULL);
    if (result != BDM_RESULT_SUCCESS) {
        BDM_Log(BDM_LOG_LEVEL_WARN, "Core: FlutterBridge initialization returned %d", result);
    }

    result = HookManager::Instance().RegisterWndProcHook(
        g_targetHwnd,
        [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, void* user_data) {
            if (msg == WM_APPCOMMAND) {
                DWORD cmd = GET_APPCOMMAND_LPARAM(lParam);
                BDM_Log(BDM_LOG_LEVEL_INFO, "Core: WM_APPCOMMAND cmd=%lu", cmd);

                switch (cmd) {
                    case APPCOMMAND_MEDIA_PLAY_PAUSE:
                        BDM_Log(BDM_LOG_LEVEL_INFO, "Core: Media Play/Pause");
                        FlutterBridge::Instance().CallMethod("playPause", NULL, NULL);
                        break;
                    case APPCOMMAND_MEDIA_STOP:
                        BDM_Log(BDM_LOG_LEVEL_INFO, "Core: Media Stop");
                        FlutterBridge::Instance().CallMethod("stop", NULL, NULL);
                        break;
                    case APPCOMMAND_MEDIA_PREVIOUS_TRACK:
                        BDM_Log(BDM_LOG_LEVEL_INFO, "Core: Media Previous");
                        FlutterBridge::Instance().CallMethod("previous", NULL, NULL);
                        break;
                    case APPCOMMAND_MEDIA_NEXT_TRACK:
                        BDM_Log(BDM_LOG_LEVEL_INFO, "Core: Media Next");
                        FlutterBridge::Instance().CallMethod("next", NULL, NULL);
                        break;
                }
            }
        },
        NULL,
        0
    );

    if (result <= 0) {
        BDM_Log(BDM_LOG_LEVEL_WARN, "Core: Failed to register WndProc hook");
    }

    result = PluginManager::Instance().Initialize(g_pluginDir, g_targetHwnd, NULL);
    if (result != BDM_RESULT_SUCCESS) {
        BDM_Log(BDM_LOG_LEVEL_ERROR, "Core: PluginManager initialization failed: %d", result);
        return result;
    }

    g_initialized = TRUE;
    BDM_Log(BDM_LOG_LEVEL_INFO, "BetterBDM Core initialized successfully");
    return BDM_RESULT_SUCCESS;
}

BDM_API void Core_Uninitialize() {
    if (!g_initialized) return;

    BDM_Log(BDM_LOG_LEVEL_INFO, "BetterBDM Core uninitializing...");

    PluginManager::Instance().Uninitialize();
    FlutterBridge::Instance().Uninitialize();
    HookManager::Instance().RemoveAllHooks();

    g_initialized = FALSE;
    g_targetHwnd = NULL;

    BDM_Log(BDM_LOG_LEVEL_INFO, "BetterBDM Core uninitialized");
}

BDM_API BDM_RESULT Core_GetVersion(char* version, int size) {
    if (!version || size < strlen(CORE_VERSION) + 1) {
        return BDM_RESULT_ERROR_INVALID_PARAM;
    }
    strncpy(version, CORE_VERSION, size);
    return BDM_RESULT_SUCCESS;
}

BDM_API BDM_RESULT Core_LoadPlugin(const char* plugin_name) {
    if (!g_initialized) return BDM_RESULT_ERROR_GENERAL;
    return PluginManager::Instance().LoadPlugin(plugin_name);
}

BDM_API BDM_RESULT Core_UnloadPlugin(const char* plugin_name) {
    if (!g_initialized) return BDM_RESULT_ERROR_GENERAL;
    return PluginManager::Instance().UnloadPlugin(plugin_name);
}

BDM_API BDM_RESULT Core_EnablePlugin(const char* plugin_name) {
    if (!g_initialized) return BDM_RESULT_ERROR_GENERAL;
    return PluginManager::Instance().EnablePlugin(plugin_name);
}

BDM_API BDM_RESULT Core_DisablePlugin(const char* plugin_name) {
    if (!g_initialized) return BDM_RESULT_ERROR_GENERAL;
    return PluginManager::Instance().DisablePlugin(plugin_name);
}

}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)hinstDLL;
    (void)lpvReserved;

    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hinstDLL);
            BDM_Log(BDM_LOG_LEVEL_INFO, "BetterBDM Core DLL attached");
            break;

        case DLL_PROCESS_DETACH:
            Core_Uninitialize();
            BDM_Log(BDM_LOG_LEVEL_INFO, "BetterBDM Core DLL detached");
            break;

        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }

    return TRUE;
}
