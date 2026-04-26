#include "..\..\include\bdmicros.h"
#include "..\..\include\plugin_api.h"

#define WM_APPCOMMAND 0x0318
#define APPCOMMAND_MEDIA_PLAY_PAUSE 14
#define APPCOMMAND_MEDIA_STOP       13
#define APPCOMMAND_MEDIA_PREVIOUS_TRACK 12
#define APPCOMMAND_MEDIA_NEXT_TRACK     11
#define GET_APPCOMMAND_LPARAM(lParam) ((DWORD)((lParam) & ~0xF))

static PluginInterface* g_pluginInterface = NULL;
static HWND g_targetHwnd = NULL;
static int g_hookId = -1;

static void MediaControl_Log(int level, const char* format, ...) {
    if (!g_pluginInterface || !g_pluginInterface->log) return;

    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    g_pluginInterface->log(level, buffer);
}

static void OnMediaKey(WPARAM wParam, LPARAM lParam) {
    DWORD cmd = GET_APPCOMMAND_LPARAM(lParam);

    switch (cmd) {
        case APPCOMMAND_MEDIA_PLAY_PAUSE:
            MediaControl_Log(BDM_LOG_LEVEL_INFO, "Plugin: Play/Pause pressed");
            g_pluginInterface->call_flutter_method("playPause", NULL, NULL);
            break;

        case APPCOMMAND_MEDIA_STOP:
            MediaControl_Log(BDM_LOG_LEVEL_INFO, "Plugin: Stop pressed");
            g_pluginInterface->call_flutter_method("stop", NULL, NULL);
            break;

        case APPCOMMAND_MEDIA_PREVIOUS_TRACK:
            MediaControl_Log(BDM_LOG_LEVEL_INFO, "Plugin: Previous pressed");
            g_pluginInterface->call_flutter_method("previous", NULL, NULL);
            break;

        case APPCOMMAND_MEDIA_NEXT_TRACK:
            MediaControl_Log(BDM_LOG_LEVEL_INFO, "Plugin: Next pressed");
            g_pluginInterface->call_flutter_method("next", NULL, NULL);
            break;

        default:
            MediaControl_Log(BDM_LOG_LEVEL_DEBUG, "Plugin: Unknown app command: %lu", cmd);
            break;
    }
}

static void HookCallback(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, void* user_data) {
    (void)hwnd;
    (void)user_data;

    if (msg == WM_APPCOMMAND) {
        OnMediaKey(wParam, lParam);
    }
}

BOOL Plugin_Initialize(PluginEnv* env) {
    if (!env) return FALSE;

    g_pluginInterface = (PluginInterface*)env;
    g_targetHwnd = env->target_hwnd;

    MediaControl_Log(BDM_LOG_LEVEL_INFO, "MediaControl Plugin initializing...");
    MediaControl_Log(BDM_LOG_LEVEL_INFO, "MediaControl: Target HWND = %p", g_targetHwnd);

    HookInfo info;
    memset(&info, 0, sizeof(info));
    info.callback = (void*)HookCallback;
    info.user_data = NULL;
    info.priority = 100;

    g_hookId = g_pluginInterface->register_hook("WM_APPCOMMAND", HookCallback, NULL, 100);
    if (g_hookId < 0) {
        MediaControl_Log(BDM_LOG_LEVEL_ERROR, "MediaControl: Failed to register hook");
        return FALSE;
    }

    MediaControl_Log(BDM_LOG_LEVEL_INFO, "MediaControl: Hook registered with id=%d", g_hookId);
    return TRUE;
}

void Plugin_Unload() {
    MediaControl_Log(BDM_LOG_LEVEL_INFO, "MediaControl Plugin unloading...");

    if (g_hookId >= 0 && g_pluginInterface) {
        g_pluginInterface->unregister_hook(g_hookId);
        g_hookId = -1;
    }

    g_pluginInterface = NULL;
    g_targetHwnd = NULL;
}

void Plugin_Enable() {
    MediaControl_Log(BDM_LOG_LEVEL_INFO, "MediaControl: Plugin enabled");
}

void Plugin_Disable() {
    MediaControl_Log(BDM_LOG_LEVEL_INFO, "MediaControl: Plugin disabled");
}

const char* Plugin_GetName() {
    return "media_control";
}

const char* Plugin_GetVersion() {
    return "1.0.0";
}

PluginInterface* Plugin_GetAPI(void* reserved) {
    (void)reserved;
    return g_pluginInterface;
}
