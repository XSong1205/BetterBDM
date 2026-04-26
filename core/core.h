#pragma once

#include "..\include\bdmicros.h"
#include "hook_manager.h"
#include "plugin_manager.h"
#include "flutter_bridge.h"

#define CORE_VERSION "1.0.0"

extern "C" {
    BDM_API BDM_RESULT Core_Initialize(BDM_StartupInfo* info);
    BDM_API void Core_Uninitialize();
    BDM_API BDM_RESULT Core_GetVersion(char* version, int size);
    BDM_API BDM_RESULT Core_LoadPlugin(const char* plugin_name);
    BDM_API BDM_RESULT Core_UnloadPlugin(const char* plugin_name);
    BDM_API BDM_RESULT Core_EnablePlugin(const char* plugin_name);
    BDM_API BDM_RESULT Core_DisablePlugin(const char* plugin_name);
};
