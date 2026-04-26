#include "plugin_manager.h"
#include "flutter_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CONFIG_LINE 512

PluginManager::PluginManager() : m_targetHwnd(NULL), m_flutterEngine(NULL), m_initialized(FALSE), m_nextHookId(1) {
    InitializeCriticalSection(&m_cs);
}

PluginManager::~PluginManager() {
    Uninitialize();
    DeleteCriticalSection(&m_cs);
}

PluginManager& PluginManager::Instance() {
    static PluginManager instance;
    return instance;
}

void PluginManager::PluginLogCallback(int level, const char* message) {
    BDM_Log(level, "[Plugin] %s", message);
}

BDM_RESULT PluginManager::Initialize(const wchar_t* plugin_dir, HWND target_hwnd, void* flutter_engine) {
    if (m_initialized) return BDM_RESULT_SUCCESS;

    wcsncpy(m_pluginDir, plugin_dir ? plugin_dir : L"./plugins", MAX_PATH - 1);
    m_pluginDir[MAX_PATH - 1] = L'\0';
    m_targetHwnd = target_hwnd;
    m_flutterEngine = flutter_engine;

    BDM_RESULT result = ScanPluginDirectory();
    if (result != BDM_RESULT_SUCCESS) {
        BDM_Log(BDM_LOG_LEVEL_WARN, "PluginManager: No plugins found or error scanning directory");
    }

    m_initialized = TRUE;
    BDM_Log(BDM_LOG_LEVEL_INFO, "PluginManager: Initialized with plugin_dir=%ls", m_pluginDir);
    return BDM_RESULT_SUCCESS;
}

BDM_RESULT PluginManager::Uninitialize() {
    if (!m_initialized) return BDM_RESULT_SUCCESS;

    EnterCriticalSection(&m_cs);

    for (auto plugin : m_plugins) {
        if (plugin->state == PLUGIN_STATE_ENABLED) {
            if (plugin->api && plugin->api->on_disable) {
                plugin->api->on_disable();
            }
        }
        if (plugin->state != PLUGIN_STATE_UNLOADED) {
            if (plugin->api && plugin->api->on_unload) {
                plugin->api->on_unload();
            }
        }
        if (plugin->dll) {
            FreeLibrary(plugin->dll);
            plugin->dll = NULL;
        }
        delete plugin;
    }
    m_plugins.clear();

    LeaveCriticalSection(&m_cs);

    m_initialized = FALSE;
    BDM_Log(BDM_LOG_LEVEL_INFO, "PluginManager: Uninitialized");
    return BDM_RESULT_SUCCESS;
}

BDM_RESULT PluginManager::ScanPluginDirectory() {
    WIN32_FIND_DATAW find_data;
    wchar_t search_path[MAX_PATH];
    swprintf(search_path, MAX_PATH, L"%s/*", m_pluginDir);

    HANDLE hFind = FindFirstFileW(search_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        return BDM_RESULT_ERROR_NOT_FOUND;
    }

    BDM_Log(BDM_LOG_LEVEL_INFO, "PluginManager: Scanning %ls", m_pluginDir);

    do {
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (wcscmp(find_data.cFileName, L".") == 0 || wcscmp(find_data.cFileName, L"..") == 0) {
                continue;
            }

            wchar_t manifest_path[MAX_PATH];
            swprintf(manifest_path, MAX_PATH, L"%s/%s/plugin.json", m_pluginDir, find_data.cFileName);

            char manifest_path_a[MAX_PATH];
            WideCharToMultiByte(CP_ACP, 0, manifest_path, -1, manifest_path_a, MAX_PATH, NULL, NULL);

            PluginManifest manifest;
            if (ParseManifest(manifest_path_a, &manifest) == BDM_RESULT_SUCCESS) {
                m_availablePlugins.push_back(manifest);
                BDM_Log(BDM_LOG_LEVEL_INFO, "PluginManager: Found plugin %s v%s", manifest.name, manifest.version);
            }
        }
    } while (FindNextFileW(hFind, &find_data));

    FindClose(hFind);
    return BDM_RESULT_SUCCESS;
}

BDM_RESULT PluginManager::ParseManifest(const char* manifest_path, PluginManifest* manifest) {
    FILE* f = fopen(manifest_path, "r");
    if (!f) return BDM_RESULT_ERROR_NOT_FOUND;

    char buffer[MAX_CONFIG_LINE];
    memset(manifest, 0, sizeof(PluginManifest));

    BOOL in_object = FALSE;
    BOOL expect_comma = FALSE;

    while (fgets(buffer, sizeof(buffer), f)) {
        char* line = buffer;
        while (*line == ' ' || *line == '\t') line++;

        char* end = line + strlen(line) - 1;
        while (end > line && (*end == '\n' || *end == '\r' || *end == ' ' || *end == '\t')) *end-- = '\0';

        if (strcmp(line, "{") == 0) {
            in_object = TRUE;
            continue;
        }
        if (strcmp(line, "}") == 0) {
            break;
        }

        if (!in_object) continue;

        char* colon = strchr(line, ':');
        if (!colon) continue;

        *colon = '\0';
        char* key = line;
        char* value = colon + 1;

        while (*key == ' ' || *key == '\t') key++;
        char* key_end = key + strlen(key) - 1;
        while (key_end > key && (*key_end == ' ' || *key_end == '\t')) *key_end-- = '\0';

        while (*value == ' ' || *value == '\t') value++;
        if (*value == '"') value++;
        char* value_end = value + strlen(value) - 1;
        if (value_end > value && *value_end == '"') *value_end-- = '\0';

        if (strcmp(key, "name") == 0) {
            strncpy(manifest->name, value, sizeof(manifest->name) - 1);
        } else if (strcmp(key, "version") == 0) {
            strncpy(manifest->version, value, sizeof(manifest->version) - 1);
        } else if (strcmp(key, "author") == 0) {
            strncpy(manifest->author, value, sizeof(manifest->author) - 1);
        } else if (strcmp(key, "description") == 0) {
            strncpy(manifest->description, value, sizeof(manifest->description) - 1);
        } else if (strcmp(key, "main") == 0) {
            strncpy(manifest->main_dll, value, sizeof(manifest->main_dll) - 1);
        } else if (strcmp(key, "api_version") == 0) {
            strncpy(manifest->api_version, value, sizeof(manifest->api_version) - 1);
        }
    }

    fclose(f);

    if (manifest->name[0] == '\0') {
        return BDM_RESULT_ERROR_GENERAL;
    }

    return BDM_RESULT_SUCCESS;
}

PluginInterface* PluginManager::CreatePluginInterface(LoadedPlugin* plugin) {
    PluginInterface* api = new PluginInterface();
    memset(api, 0, sizeof(PluginInterface));

    api->api_version = 1;

    api->on_load = NULL;
    api->on_unload = NULL;
    api->on_enable = NULL;
    api->on_disable = NULL;
    api->get_name = NULL;
    api->get_version = NULL;

    api->register_hook = [](const char* hook_type, HookCallback callback, void* user_data, int priority) -> int {
        HookInfo info;
        memset(&info, 0, sizeof(info));
        info.callback = (void*)callback;
        info.user_data = user_data;
        info.priority = priority;
        return PluginManager::Instance().RegisterHook(&info);
    };

    api->unregister_hook = [](int hook_id) -> int {
        return PluginManager::Instance().UnregisterHook(hook_id) ? 0 : -1;
    };

    api->call_flutter_method = [](const char* method, const char* args, char** result) -> int {
        return FlutterBridge::Instance().CallMethod(method, args, result) ? 0 : -1;
    };

    api->listen_flutter_event = [](const char* channel, void (*callback)(const char*, void*), void* user_data) -> int {
        return FlutterBridge::Instance().ListenEvent(channel, callback, user_data) ? 0 : -1;
    };

    api->log = [](int level, const char* format, ...) {
        char buffer[1024];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        PluginLogCallback(level, buffer);
    };

    api->malloc = malloc;
    api->free = free;

    api->user_data = plugin;

    return api;
}

BDM_RESULT PluginManager::LoadPlugin(const char* plugin_name) {
    EnterCriticalSection(&m_cs);

    for (auto plugin : m_plugins) {
        if (strcmp(plugin->manifest.name, plugin_name) == 0) {
            LeaveCriticalSection(&m_cs);
            return BDM_RESULT_ERROR_ALREADY_EXISTS;
        }
    }

    PluginManifest* manifest = NULL;
    for (auto& m : m_availablePlugins) {
        if (strcmp(m.name, plugin_name) == 0) {
            manifest = &m;
            break;
        }
    }

    if (!manifest) {
        LeaveCriticalSection(&m_cs);
        return BDM_RESULT_ERROR_NOT_FOUND;
    }

    LoadedPlugin* plugin = new LoadedPlugin();
    memset(plugin, 0, sizeof(LoadedPlugin));
    strncpy(plugin->manifest.name, manifest->name, sizeof(plugin->manifest.name) - 1);
    strncpy(plugin->manifest.version, manifest->version, sizeof(plugin->manifest.version) - 1);
    strncpy(plugin->manifest.main_dll, manifest->main_dll, sizeof(plugin->manifest.main_dll) - 1);
    plugin->state = PLUGIN_STATE_LOADED;

    char dll_path[MAX_PATH];
    snprintf(dll_path, sizeof(dll_path), "%s/%s/%s", m_pluginDir, plugin_name, plugin->manifest.main_dll);

    plugin->dll = LoadLibraryA(dll_path);
    if (!plugin->dll) {
        BDM_Log(BDM_LOG_LEVEL_ERROR, "PluginManager: Failed to load %s, error=%lu", dll_path, GetLastError());
        delete plugin;
        LeaveCriticalSection(&m_cs);
        return BDM_RESULT_ERROR_GENERAL;
    }

    FARPROC init_proc = GetProcAddress(plugin->dll, PLUGIN_INIT_EXPORT_NAME);
    if (!init_proc) {
        BDM_Log(BDM_LOG_LEVEL_ERROR, "PluginManager: Plugin does not export %s", PLUGIN_INIT_EXPORT_NAME);
        FreeLibrary(plugin->dll);
        delete plugin;
        LeaveCriticalSection(&m_cs);
        return BDM_RESULT_ERROR_GENERAL;
    }

    PluginEnv env;
    memset(&env, 0, sizeof(env));
    env.target_hwnd = m_targetHwnd;
    env.core_hwnd = NULL;
    env.target_pid = GetCurrentProcessId();
    env.plugin_dir = m_pluginDir;
    env.flutter_engine = m_flutterEngine;
    env.env = &env;

    if (!((Plugin_OnLoad)init_proc)(&env)) {
        BDM_Log(BDM_LOG_LEVEL_ERROR, "PluginManager: Plugin %s on_load returned false", plugin_name);
        FreeLibrary(plugin->dll);
        delete plugin;
        LeaveCriticalSection(&m_cs);
        return BDM_RESULT_ERROR_GENERAL;
    }

    plugin->api = CreatePluginInterface(plugin);

    FARPROC get_api_proc = GetProcAddress(plugin->dll, PLUGIN_GET_API_EXPORT_NAME);
    if (get_api_proc) {
        PluginExports exports;
        exports.api_version = 1;
        exports.get_api = (Plugin_GetAPIFn)get_api_proc;
        PluginInterface* external_api = exports.get_api(NULL);
        if (external_api) {
            plugin->api = external_api;
        }
    }

    m_plugins.push_back(plugin);
    LeaveCriticalSection(&m_cs);

    BDM_Log(BDM_LOG_LEVEL_INFO, "PluginManager: Loaded plugin %s", plugin_name);
    return BDM_RESULT_SUCCESS;
}

BDM_RESULT PluginManager::UnloadPlugin(const char* plugin_name) {
    EnterCriticalSection(&m_cs);

    auto it = m_plugins.begin();
    while (it != m_plugins.end()) {
        LoadedPlugin* plugin = *it;
        if (strcmp(plugin->manifest.name, plugin_name) == 0) {
            if (plugin->state == PLUGIN_STATE_ENABLED) {
                DisablePlugin(plugin_name);
            }

            RemoveHooksForPlugin(plugin);

            if (plugin->api && plugin->api->on_unload) {
                plugin->api->on_unload();
            }

            if (plugin->dll) {
                FreeLibrary(plugin->dll);
            }

            delete plugin;
            m_plugins.erase(it);
            LeaveCriticalSection(&m_cs);
            BDM_Log(BDM_LOG_LEVEL_INFO, "PluginManager: Unloaded plugin %s", plugin_name);
            return BDM_RESULT_SUCCESS;
        }
        ++it;
    }

    LeaveCriticalSection(&m_cs);
    return BDM_RESULT_ERROR_NOT_FOUND;
}

BDM_RESULT PluginManager::EnablePlugin(const char* plugin_name) {
    EnterCriticalSection(&m_cs);

    for (auto plugin : m_plugins) {
        if (strcmp(plugin->manifest.name, plugin_name) == 0) {
            if (plugin->state == PLUGIN_STATE_ENABLED) {
                LeaveCriticalSection(&m_cs);
                return BDM_RESULT_SUCCESS;
            }

            if (plugin->api && plugin->api->on_enable) {
                plugin->api->on_enable();
            }

            plugin->state = PLUGIN_STATE_ENABLED;
            LeaveCriticalSection(&m_cs);
            BDM_Log(BDM_LOG_LEVEL_INFO, "PluginManager: Enabled plugin %s", plugin_name);
            return BDM_RESULT_SUCCESS;
        }
    }

    LeaveCriticalSection(&m_cs);
    return BDM_RESULT_ERROR_NOT_FOUND;
}

BDM_RESULT PluginManager::DisablePlugin(const char* plugin_name) {
    EnterCriticalSection(&m_cs);

    for (auto plugin : m_plugins) {
        if (strcmp(plugin->manifest.name, plugin_name) == 0) {
            if (plugin->state != PLUGIN_STATE_ENABLED) {
                LeaveCriticalSection(&m_cs);
                return BDM_RESULT_SUCCESS;
            }

            if (plugin->api && plugin->api->on_disable) {
                plugin->api->on_disable();
            }

            plugin->state = PLUGIN_STATE_DISABLED;

            for (int hook_id : plugin->registered_hooks) {
                HookManager::Instance().DisableHook(hook_id);
            }

            LeaveCriticalSection(&m_cs);
            BDM_Log(BDM_LOG_LEVEL_INFO, "PluginManager: Disabled plugin %s", plugin_name);
            return BDM_RESULT_SUCCESS;
        }
    }

    LeaveCriticalSection(&m_cs);
    return BDM_RESULT_ERROR_NOT_FOUND;
}

BDM_RESULT PluginManager::ReloadPlugin(const char* plugin_name) {
    BDM_RESULT result = UnloadPlugin(plugin_name);
    if (result != BDM_RESULT_SUCCESS) {
        return result;
    }
    return LoadPlugin(plugin_name);
}

void PluginManager::ListPlugins(std::list<LoadedPlugin*>& plugins) {
    EnterCriticalSection(&m_cs);
    plugins = m_plugins;
    LeaveCriticalSection(&m_cs);
}

int PluginManager::RegisterHook(HookInfo* info) {
    if (!info) return -1;

    EnterCriticalSection(&m_cs);
    int hook_id = m_nextHookId++;
    LeaveCriticalSection(&m_cs);

    info->hook_id = hook_id;
    info->type = BDM_HOOK_WNDPROC;

    if (m_targetHwnd && info->callback) {
        int id = HookManager::Instance().RegisterWndProcHook(
            m_targetHwnd,
            (HookProc)info->callback,
            info->user_data,
            info->priority
        );

        if (id > 0) {
            EnterCriticalSection(&m_cs);
            for (auto plugin : m_plugins) {
                plugin->registered_hooks.push_back(id);
            }
            LeaveCriticalSection(&m_cs);
            return id;
        }
    }

    return -1;
}

int PluginManager::UnregisterHook(int hook_id) {
    HookManager::Instance().UnregisterHook(hook_id);

    EnterCriticalSection(&m_cs);
    for (auto plugin : m_plugins) {
        plugin->registered_hooks.remove(hook_id);
    }
    LeaveCriticalSection(&m_cs);

    return 0;
}

void PluginManager::RemoveHooksForPlugin(LoadedPlugin* plugin) {
    for (int hook_id : plugin->registered_hooks) {
        HookManager::Instance().UnregisterHook(hook_id);
    }
    plugin->registered_hooks.clear();
}
