#include "injector.h"
#include <stdio.h>
#include <string.h>

void Injector_PrintUsage(const char* prog_name) {
    printf("BetterBDM Injector v%s\n", INJECTOR_VERSION);
    printf("\n");
    printf("Usage:\n");
    printf("  %s --attach [options]    Attach to target process and inject\n", prog_name);
    printf("  %s --detach [options]     Detach core.dll from target process\n", prog_name);
    printf("  %s --list                 List available plugins\n", prog_name);
    printf("\n");
    printf("Options:\n");
    printf("  -t, --title <title>       Target window title (default: 波点音乐)\n");
    printf("  -p, --process <name>      Target process name\n");
    printf("  -c, --core <path>         Path to core.dll (default: ./build/core.dll)\n");
    printf("  -d, --plugins <path>      Plugin directory (default: ./plugins)\n");
    printf("  -v, --verbose             Verbose output\n");
    printf("  -h, --help                Show this help\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s --attach -t \"波点音乐\"\n", prog_name);
    printf("  %s --attach -p \"bdmusic.exe\"\n", prog_name);
    printf("  %s --detach -t \"波点音乐\"\n", prog_name);
}

BOOL Injector_ParseArgs(int argc, char* argv[], InjectorConfig* config) {
    if (!config) return FALSE;

    memset(config, 0, sizeof(InjectorConfig));

    strncpy(config->target_title, "波点音乐", sizeof(config->target_title) - 1);
    strncpy(config->core_dll, "./build/core.dll", sizeof(config->core_dll) - 1);
    strncpy(config->plugin_dir, "./plugins", sizeof(config->plugin_dir) - 1);
    config->auto_inject = FALSE;
    config->verbose = FALSE;

    if (argc < 2) {
        return FALSE;
    }

    BOOL is_attach = FALSE;
    BOOL is_detach = FALSE;
    BOOL is_list = FALSE;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--attach") == 0 || strcmp(argv[i], "-a") == 0) {
            is_attach = TRUE;
        } else if (strcmp(argv[i], "--detach") == 0 || strcmp(argv[i], "-d") == 0) {
            is_detach = TRUE;
        } else if (strcmp(argv[i], "--list") == 0 || strcmp(argv[i], "-l") == 0) {
            is_list = TRUE;
        } else if ((strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--title") == 0) && i + 1 < argc) {
            strncpy(config->target_title, argv[++i], sizeof(config->target_title) - 1);
        } else if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--process") == 0) && i + 1 < argc) {
            strncpy(config->target_process, argv[++i], sizeof(config->target_process) - 1);
        } else if ((strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--core") == 0) && i + 1 < argc) {
            strncpy(config->core_dll, argv[++i], sizeof(config->core_dll) - 1);
        } else if ((strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--plugins") == 0) && i + 1 < argc) {
            strncpy(config->plugin_dir, argv[++i], sizeof(config->plugin_dir) - 1);
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            config->verbose = TRUE;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            return FALSE;
        }
    }

    if (is_attach) config->auto_inject = TRUE;
    if (is_list) return TRUE;
    if (!is_attach && !is_detach) return FALSE;

    return TRUE;
}

BOOL Injector_IsAdmin() {
    BOOL is_admin = FALSE;
    PSID admin_group = NULL;
    SID_IDENTIFIER_AUTHORITY authority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(&authority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0, &admin_group)) {
        CheckTokenMembership(NULL, admin_group, &is_admin);
        FreeSid(admin_group);
    }

    return is_admin;
}

BOOL Injector_RequestAdmin() {
    wchar_t exe_path[MAX_PATH];
    GetModuleFileNameW(NULL, exe_path, ARRAYSIZE(exe_path));

    wchar_t args[MAX_PATH * 2];
    swprintf(args, ARRAYSIZE(args), L"\"%s\" --elevated", exe_path);

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas";
    sei.lpFile = exe_path;
    sei.lpParameters = args;
    sei.hwnd = NULL;
    sei.nShow = SW_SHOWNORMAL;

    return ShellExecuteExW(&sei);
}

BOOL Injector_Attach(ProcessInfo* process, InjectorConfig* config) {
    if (!process || !config) return FALSE;

    if (config->verbose) {
        printf("[*] Target PID: %lu\n", process->pid);
        printf("[*] Target Window: %s\n", process->window_title);
        printf("[*] Target Process: %s\n", process->process_name);
        printf("[*] Core DLL: %s\n", config->core_dll);
        printf("[*] Plugin Dir: %s\n", config->plugin_dir);
    }

    if (Process_IsLoaded(process, L"core.dll")) {
        printf("[!] core.dll is already loaded in target process.\n");
        printf("[*] Use --detach first to unload, or restart the target.\n");
        return FALSE;
    }

    wchar_t core_dll_w[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, config->core_dll, -1, core_dll_w, MAX_PATH);

    printf("[*] Injecting core.dll...\n");

    if (!Process_InjectDLL(process, core_dll_w)) {
        printf("[!] Failed to inject core.dll\n");
        printf("[!] Error: %lu\n", GetLastError());
        return FALSE;
    }

    printf("[+] core.dll injected successfully!\n");

    if (config->verbose) {
        if (Process_IsLoaded(process, L"core.dll")) {
            printf("[+] Verification: core.dll is loaded in target process.\n");
        }
    }

    return TRUE;
}

BOOL Injector_Detach(ProcessInfo* process, InjectorConfig* config) {
    if (!process || !config) return FALSE;

    printf("[*] Detaching core.dll from PID %lu...\n", process->pid);

    if (!Process_IsLoaded(process, L"core.dll")) {
        printf("[!] core.dll is not loaded in target process.\n");
        return FALSE;
    }

    if (!Process_EjectDLL(process, L"core.dll")) {
        printf("[!] Failed to detach core.dll\n");
        return FALSE;
    }

    printf("[+] core.dll detached successfully!\n");
    return TRUE;
}

BOOL Injector_ListPlugins(InjectorConfig* config) {
    if (!config) return FALSE;

    char plugin_dir[MAX_PATH];
    if (config->plugin_dir[0] == '.' && config->plugin_dir[1] == '/') {
        GetCurrentDirectoryA(MAX_PATH, plugin_dir);
        strncat(plugin_dir, config->plugin_dir + 1, MAX_PATH - strlen(plugin_dir) - 1);
    } else {
        strncpy(plugin_dir, config->plugin_dir, MAX_PATH - 1);
    }

    printf("[*] Scanning plugin directory: %s\n", plugin_dir);

    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(plugin_dir, &find_data);

    if (hFind == INVALID_HANDLE_VALUE) {
        printf("[!] Plugin directory not found.\n");
        return FALSE;
    }

    FindClose(hFind);

    strncat(plugin_dir, "/*", MAX_PATH - strlen(plugin_dir) - 1);
    hFind = FindFirstFileA(plugin_dir, &find_data);

    if (hFind == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    printf("\n[+] Available plugins:\n");
    printf("----------------------\n");

    int count = 0;
    do {
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
                continue;
            }

            char manifest_path[MAX_PATH];
            snprintf(manifest_path, sizeof(manifest_path), "%s/%s/plugin.json",
                config->plugin_dir[0] == '.' ? config->plugin_dir + 2 : config->plugin_dir,
                find_data.cFileName);

            if (GetFileAttributesA(manifest_path) != INVALID_FILE_ATTRIBUTES) {
                printf("  [%d] %s\n", ++count, find_data.cFileName);
            }
        }
    } while (FindNextFileA(hFind, &find_data));

    FindClose(hFind);

    if (count == 0) {
        printf("  No plugins found.\n");
    }

    printf("\n");
    return TRUE;
}

int main(int argc, char* argv[]) {
    printf("\n");
    printf("========================================\n");
    printf("  BetterBDM - Windows Injection Framework\n");
    printf("  Version: %s\n", INJECTOR_VERSION);
    printf("========================================\n");
    printf("\n");

    if (!Injector_IsAdmin()) {
        printf("[*] Administrator privileges recommended for injection.\n");
        printf("[*] Some operations may fail without admin rights.\n\n");
    }

    InjectorConfig config;
    if (!Injector_ParseArgs(argc, argv, &config)) {
        Injector_PrintUsage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "--list") == 0 || strcmp(argv[1], "-l") == 0) {
        Injector_ListPlugins(&config);
        return 0;
    }

    ProcessInfo process;
    BOOL found = FALSE;

    if (config.target_title[0] != '\0') {
        wchar_t title_w[256];
        MultiByteToWideChar(CP_ACP, 0, config.target_title, -1, title_w, 256);
        if (Process_FindByWindowTitle(title_w, &process)) {
            found = TRUE;
            if (config.verbose) {
                printf("[*] Found target by window title: %s\n", config.target_title);
            }
        }
    }

    if (!found && config.target_process[0] != '\0') {
        if (Process_FindByName(config.target_process, &process)) {
            found = TRUE;
            if (config.verbose) {
                printf("[*] Found target by process name: %s\n", config.target_process);
            }
        }
    }

    if (!found) {
        printf("[!] Target process not found.\n");
        printf("[*] Please ensure the target application is running.\n");
        return 1;
    }

    int result = 0;

    if (config.auto_inject) {
        if (!Injector_Attach(&process, &config)) {
            result = 1;
        }
    } else {
        printf("[*] Process found: %s (PID: %lu)\n", process.process_name, process.pid);
        printf("[*] Use --attach to inject or --detach to unload.\n");
    }

    Process_Close(&process);
    return result;
}
