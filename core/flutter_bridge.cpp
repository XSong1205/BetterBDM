#include "flutter_bridge.h"
#include <tchar.h>

FlutterBridge::FlutterBridge() : m_targetHwnd(NULL), m_flutterHwnd(NULL), m_enginePtr(NULL), m_originalWndProc(NULL), m_initialized(FALSE) {
    InitializeCriticalSection(&m_cs);
}

FlutterBridge::~FlutterBridge() {
    Uninitialize();
    DeleteCriticalSection(&m_cs);
}

FlutterBridge& FlutterBridge::Instance() {
    static FlutterBridge instance;
    return instance;
}

BDM_RESULT FlutterBridge::Initialize(HWND target_hwnd, void* engine_ptr) {
    if (m_initialized) return BDM_RESULT_SUCCESS;

    m_targetHwnd = target_hwnd;

    if (!engine_ptr) {
        m_flutterHwnd = FindFlutterWindow();
        if (m_flutterHwnd) {
            m_enginePtr = FindFlutterEngine();
        }
    } else {
        m_enginePtr = engine_ptr;
    }

    if (!m_targetHwnd) {
        BDM_Log(BDM_LOG_LEVEL_WARN, "FlutterBridge: Target window not found");
    }

    if (m_flutterHwnd && !m_originalWndProc) {
        m_originalWndProc = (WNDPROC)SetWindowLongPtr(m_flutterHwnd, GWLP_WNDPROC, (LONG_PTR)SubclassProc);
    }

    m_initialized = TRUE;
    BDM_Log(BDM_LOG_LEVEL_INFO, "FlutterBridge: Initialized, engine=%p, flutter_hwnd=%p", m_enginePtr, m_flutterHwnd);
    return BDM_RESULT_SUCCESS;
}

void FlutterBridge::Uninitialize() {
    if (!m_initialized) return;

    if (m_flutterHwnd && m_originalWndProc) {
        SetWindowLongPtr(m_flutterHwnd, GWLP_WNDPROC, (LONG_PTR)m_originalWndProc);
        m_originalWndProc = NULL;
    }

    EnterCriticalSection(&m_cs);
    for (auto listener : m_listeners) {
        delete listener;
    }
    m_listeners.clear();
    LeaveCriticalSection(&m_cs);

    m_initialized = FALSE;
    BDM_Log(BDM_LOG_LEVEL_INFO, "FlutterBridge: Uninitialized");
}

HWND FlutterBridge::FindFlutterWindow() {
    HWND hwnd = FindWindowW(L"FlutterView", NULL);
    if (hwnd) return hwnd;

    hwnd = FindWindowExW(NULL, NULL, L"FlutterView", NULL);
    if (hwnd) return hwnd;

    DWORD pid = 0;
    if (m_targetHwnd) {
        GetWindowThreadProcessId(m_targetHwnd, &pid);
    }

    if (pid == 0) return NULL;

    HWND result = NULL;
    EnumChildWindows(m_targetHwnd, [](HWND hwnd, LPARAM lParam) -> BOOL {
        TCHAR class_name[64];
        if (GetClassName(hwnd, class_name, 64)) {
            if (_tcsstr(class_name, L"Flutter") != NULL) {
                *(HWND*)lParam = hwnd;
                return FALSE;
            }
        }
        return TRUE;
    }, (LPARAM)&result);

    return result;
}

void* FlutterBridge::FindFlutterEngine() {
    if (!m_flutterHwnd) return NULL;

    DWORD pid;
    GetWindowThreadProcessId(m_flutterHwnd, &pid);

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProcess) return NULL;

    HMODULE hModules[1024];
    DWORD bytes_needed;

    if (!EnumProcessModules(hProcess, hModules, sizeof(hModules), &bytes_needed)) {
        CloseHandle(hProcess);
        return NULL;
    }

    void* engine_ptr = NULL;
    DWORD num_modules = bytes_needed / sizeof(HMODULE);

    for (DWORD i = 0; i < num_modules; i++) {
        HMODULE hModule = hModules[i];
        char module_name[MAX_PATH];

        if (GetModuleBaseNameA(hProcess, hModule, module_name, sizeof(module_name))) {
            if (strstr(module_name, "flutter") && strstr(module_name, "engine")) {
                MODULEINFO mod_info;
                if (GetModuleInformation(hProcess, hModule, &mod_info, sizeof(mod_info))) {
                    BYTE header[4096];
                    SIZE_T bytes_read;

                    if (ReadProcessMemory(hProcess, mod_info.lpBaseOfDll, header, sizeof(header), &bytes_read)) {
                        PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)header;
                        PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((BYTE*)header + dos->e_lfanew);

                        PIMAGE_DATA_DIRECTORY export_dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
                        if (export_dir->Size > 0) {
                            DWORD export_rva = export_dir->VirtualAddress;
                            char export_name[MAX_PATH];
                            snprintf(export_name, sizeof(export_name), "FlutterEngine v%p", mod_info.lpBaseOfDll);
                            engine_ptr = mod_info.lpBaseOfDll;
                            break;
                        }
                    }
                }
            }
        }
    }

    CloseHandle(hProcess);
    return engine_ptr;
}

BOOL FlutterBridge::SetupMethodChannel() {
    return TRUE;
}

BDM_RESULT FlutterBridge::CallMethod(const char* method, const char* args, char** result) {
    if (!method) return BDM_RESULT_ERROR_INVALID_PARAM;

    BDM_Log(BDM_LOG_LEVEL_INFO, "FlutterBridge: CallMethod %s(%s)", method, args ? args : "");

    if (!m_flutterHwnd) {
        return BDM_RESULT_ERROR_NOT_FOUND;
    }

    COPYDATASTRUCT cds;
    cds.dwData = 0x746d636c;
    cds.cbData = strlen(method) + 1 + (args ? strlen(args) + 1 : 0);
    char* data = new char[cds.cbData];
    strcpy(data, method);
    if (args) {
        strcpy(data + strlen(method) + 1, args);
    } else {
        data[strlen(method)] = '\0';
    }
    cds.lpData = data;

    SendMessage(m_flutterHwnd, WM_COPYDATA, (WPARAM)m_targetHwnd, (LPARAM)&cds);

    delete[] data;

    if (result) {
        *result = NULL;
    }

    return BDM_RESULT_SUCCESS;
}

BDM_RESULT FlutterBridge::ListenEvent(const char* channel, void (*callback)(const char*, void*), void* user_data) {
    if (!channel || !callback) return BDM_RESULT_ERROR_INVALID_PARAM;

    EventListener* listener = new EventListener();
    listener->channel = channel;
    listener->callback = callback;
    listener->user_data = user_data;

    EnterCriticalSection(&m_cs);
    m_listeners.push_back(listener);
    LeaveCriticalSection(&m_cs);

    BDM_Log(BDM_LOG_LEVEL_INFO, "FlutterBridge: Listening on channel %s", channel);
    return BDM_RESULT_SUCCESS;
}

BDM_RESULT FlutterBridge::UnlistenEvent(const char* channel) {
    if (!channel) return BDM_RESULT_ERROR_INVALID_PARAM;

    EnterCriticalSection(&m_cs);
    auto it = m_listeners.begin();
    while (it != m_listeners.end()) {
        if (strcmp((*it)->channel, channel) == 0) {
            delete *it;
            m_listeners.erase(it);
            LeaveCriticalSection(&m_cs);
            return BDM_RESULT_SUCCESS;
        }
        ++it;
    }
    LeaveCriticalSection(&m_cs);

    return BDM_RESULT_ERROR_NOT_FOUND;
}

LRESULT CALLBACK FlutterBridge::SubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    FlutterBridge& bridge = FlutterBridge::Instance();

    if (msg == WM_COPYDATA) {
        COPYDATASTRUCT* cds = (COPYDATASTRUCT*)lParam;
        if (cds && cds->lpData) {
            char* data = (char*)cds->lpData;
            BDM_Log(BDM_LOG_LEVEL_DEBUG, "FlutterBridge: WM_COPYDATA received, size=%lu", cds->cbData);
        }
    }

    if (msg == WM_BDM_PLUGIN_LOAD) {
        BDM_Log(BDM_LOG_LEVEL_INFO, "FlutterBridge: Plugin load message received");
    }

    return CallWindowProc(bridge.m_originalWndProc, hwnd, msg, wParam, lParam);
}
