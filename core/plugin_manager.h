#pragma once

#include "..\include\bdmicros.h"
#include "..\include\plugin_api.h"
#include "hook_manager.h"
#include <windows.h>
#include <list>
#include <string>

class FlutterBridge;

typedef struct {
    char name[64];
    char version[16];
    char path[MAX_PATH];
    HMODULE dll;
    PluginState state;
    PluginManifest manifest;
    PluginInterface* api;
    std::list<int> registered_hooks;
} LoadedPlugin;

class PluginManager {
public:
    static PluginManager& Instance();

    BDM_RESULT Initialize(const wchar_t* plugin_dir, HWND target_hwnd, void* flutter_engine);
    BDM_RESULT Uninitialize();

    BDM_RESULT LoadPlugin(const char* plugin_name);
    BDM_RESULT UnloadPlugin(const char* plugin_name);
    BDM_RESULT EnablePlugin(const char* plugin_name);
    BDM_RESULT DisablePlugin(const char* plugin_name);

    BDM_RESULT ReloadPlugin(const char* plugin_name);

    void ListPlugins(std::list<LoadedPlugin*>& plugins);

    int RegisterHook(HookInfo* info);
    int UnregisterHook(int hook_id);

    void RemoveHooksForPlugin(LoadedPlugin* plugin);

    const wchar_t* GetPluginDir() const { return m_pluginDir; }
    HWND GetTargetHwnd() const { return m_targetHwnd; }
    void* GetFlutterEngine() const { return m_flutterEngine; }

private:
    PluginManager();
    ~PluginManager();

    BDM_RESULT ScanPluginDirectory();
    BDM_RESULT LoadPluginFromPath(const char* plugin_path);
    BDM_RESULT ParseManifest(const char* manifest_path, PluginManifest* manifest);

    PluginInterface* CreatePluginInterface(LoadedPlugin* plugin);

    static void PluginLogCallback(int level, const char* message);

    std::list<LoadedPlugin*> m_plugins;
    std::list<PluginManifest> m_availablePlugins;
    wchar_t m_pluginDir[MAX_PATH];
    HWND m_targetHwnd;
    void* m_flutterEngine;
    BOOL m_initialized;

    CRITICAL_SECTION m_cs;
    int m_nextHookId;
};
