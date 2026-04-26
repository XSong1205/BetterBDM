#pragma once

#include "process_utils.h"
#include <winhttp.h>

#define INJECTOR_VERSION "1.0.0"

typedef struct {
    char target_title[256];
    char target_process[128];
    char core_dll[MAX_PATH];
    char plugin_dir[MAX_PATH];
    BOOL auto_inject;
    BOOL verbose;
} InjectorConfig;

void Injector_PrintUsage(const char* prog_name);
BOOL Injector_ParseArgs(int argc, char* argv[], InjectorConfig* config);
BOOL Injector_LoadConfig(const char* config_path, InjectorConfig* config);

BOOL Injector_Attach(ProcessInfo* process, InjectorConfig* config);
BOOL Injector_Detach(ProcessInfo* process, InjectorConfig* config);
BOOL Injector_ListPlugins(InjectorConfig* config);

BOOL Injector_IsAdmin();
BOOL Injector_RequestAdmin();
