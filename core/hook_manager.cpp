#include "hook_manager.h"
#include <intrin.h>

HookManager::HookManager() : m_nextHookId(1), m_wndProcHookInstalled(FALSE), m_targetHwnd(NULL), m_originalWndProc(NULL) {
    InitializeCriticalSection(&m_cs);
}

HookManager::~HookManager() {
    RemoveAllHooks();
    DeleteCriticalSection(&m_cs);
}

HookManager& HookManager::Instance() {
    static HookManager instance;
    return instance;
}

int HookManager::GenerateHookId() {
    EnterCriticalSection(&m_cs);
    int id = m_nextHookId++;
    LeaveCriticalSection(&m_cs);
    return id;
}

int HookManager::FindNextPriority(BDM_HOOK_TYPE type, HWND hwnd) {
    EnterCriticalSection(&m_cs);
    int max_priority = -1;

    for (auto entry : m_hooks) {
        if (entry->type == type && entry->active) {
            if (hwnd == NULL || (type == BDM_HOOK_WNDPROC && FindHookByHwnd(hwnd) == entry)) {
                if (entry->priority > max_priority) {
                    max_priority = entry->priority;
                }
            }
        }
    }

    LeaveCriticalSection(&m_cs);
    return max_priority + 1;
}

int HookManager::RegisterHook(HookEntry* entry) {
    EnterCriticalSection(&m_cs);
    m_hooks.push_back(entry);
    LeaveCriticalSection(&m_cs);
    return entry->hook_id;
}

HookEntry* HookManager::FindHook(int hook_id) {
    EnterCriticalSection(&m_cs);
    for (auto entry : m_hooks) {
        if (entry->hook_id == hook_id) {
            LeaveCriticalSection(&m_cs);
            return entry;
        }
    }
    LeaveCriticalSection(&m_cs);
    return NULL;
}

HookEntry* HookManager::FindHookByHwnd(HWND hwnd) {
    EnterCriticalSection(&m_cs);
    for (auto entry : m_hooks) {
        if (entry->type == BDM_HOOK_WNDPROC && entry->active && entry->user_data) {
            HWND hook_hwnd = (HWND)entry->user_data;
            if (hook_hwnd == hwnd) {
                LeaveCriticalSection(&m_cs);
                return entry;
            }
        }
    }
    LeaveCriticalSection(&m_cs);
    return NULL;
}

int HookManager::RegisterWndProcHook(HWND hwnd, HookProc callback, void* user_data, int priority) {
    EnterCriticalSection(&m_cs);

    for (auto entry : m_hooks) {
        if (entry->type == BDM_HOOK_WNDPROC && entry->active && (HWND)entry->user_data == hwnd) {
            LeaveCriticalSection(&m_cs);
            return -1;
        }
    }

    if (!m_wndProcHookInstalled) {
        m_targetHwnd = hwnd;
        m_originalWndProc = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_WNDPROC);

        SetLastError(0);
        WNDPROC newWndProc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)WndProcHookProc);
        if (!newWndProc && GetLastError() != 0) {
            LeaveCriticalSection(&m_cs);
            return -1;
        }

        m_wndProcHookInstalled = TRUE;
    }

    HookEntry* entry = new HookEntry();
    memset(entry, 0, sizeof(HookEntry));
    entry->hook_id = GenerateHookId();
    entry->type = BDM_HOOK_WNDPROC;
    entry->name = "WndProc";
    entry->callback = (void*)callback;
    entry->user_data = (void*)hwnd;
    entry->priority = priority;
    entry->active = TRUE;
    entry->original_addr = (void*)m_originalWndProc;

    LeaveCriticalSection(&m_cs);

    int hook_id = RegisterHook(entry);
    BDM_Log(BDM_LOG_LEVEL_INFO, "HookManager: Registered WndProc hook id=%d for hwnd=%p", hook_id, hwnd);
    return hook_id;
}

BOOL HookManager::UnregisterHook(int hook_id) {
    EnterCriticalSection(&m_cs);

    auto it = m_hooks.begin();
    while (it != m_hooks.end()) {
        if ((*it)->hook_id == hook_id) {
            HookEntry* entry = *it;

            if (entry->type == BDM_HOOK_WNDPROC && m_wndProcHookInstalled) {
                BOOL has_other_wndproc_hooks = FALSE;
                for (auto other : m_hooks) {
                    if (other != entry && other->type == BDM_HOOK_WNDPROC && other->active) {
                        has_other_wndproc_hooks = TRUE;
                        break;
                    }
                }

                if (!has_other_wndproc_hooks) {
                    SetWindowLongPtr(m_targetHwnd, GWLP_WNDPROC, (LONG_PTR)m_originalWndProc);
                    m_wndProcHookInstalled = FALSE;
                    m_targetHwnd = NULL;
                    m_originalWndProc = NULL;
                }
            }

            m_hooks.erase(it);
            delete entry;
            LeaveCriticalSection(&m_cs);
            return TRUE;
        }
        ++it;
    }

    LeaveCriticalSection(&m_cs);
    return FALSE;
}

BOOL HookManager::EnableHook(int hook_id) {
    HookEntry* entry = FindHook(hook_id);
    if (!entry) return FALSE;

    EnterCriticalSection(&m_cs);
    entry->active = TRUE;
    LeaveCriticalSection(&m_cs);

    return TRUE;
}

BOOL HookManager::DisableHook(int hook_id) {
    HookEntry* entry = FindHook(hook_id);
    if (!entry) return FALSE;

    EnterCriticalSection(&m_cs);
    entry->active = FALSE;
    LeaveCriticalSection(&m_cs);

    return TRUE;
}

int HookManager::RegisterWinEventHook(DWORD event_min, DWORD event_max, HookProc callback, void* user_data, int priority) {
    HookEntry* entry = new HookEntry();
    memset(entry, 0, sizeof(HookEntry));
    entry->hook_id = GenerateHookId();
    entry->type = BDM_HOOK_SETWINEVENT;
    entry->name = "WinEvent";
    entry->callback = (void*)callback;
    entry->user_data = user_data;
    entry->priority = priority;
    entry->active = TRUE;

    HWINEVENTHOOK hHook = SetWinEventHook(event_min, event_max, NULL, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
    if (!hHook) {
        delete entry;
        return -1;
    }

    entry->original_addr = (void*)hHook;

    EnterCriticalSection(&m_cs);
    m_winEventHooks.push_back(hHook);
    m_hooks.push_back(entry);
    LeaveCriticalSection(&m_cs);

    BDM_Log(BDM_LOG_LEVEL_INFO, "HookManager: Registered WinEvent hook id=%d", entry->hook_id);
    return entry->hook_id;
}

int HookManager::RegisterIATHook(const char* module_name, const char* func_name, void* new_func, void** old_func) {
    HMODULE hModule = GetModuleHandleA(module_name);
    if (!hModule) return -1;

    PIMAGE_DOS_HEADER dos_header = (PIMAGE_DOS_HEADER)hModule;
    PIMAGE_NT_HEADERS nt_headers = (PIMAGE_NT_HEADERS)((BYTE*)hModule + dos_header->e_lfanew);
    PIMAGE_DATA_DIRECTORY import_dir = &nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

    if (import_dir->Size == 0) return -1;

    PIMAGE_IMPORT_DESCRIPTOR import_desc = (PIMAGE_IMPORT_DESCRIPTOR)((BYTE*)hModule + import_dir->VirtualAddress);

    while (import_desc->Name) {
        const char* mod_name = (const char*)((BYTE*)hModule + import_desc->Name);
        if (_stricmp(mod_name, module_name) == 0) {
            PIMAGE_THUNK_DATA thunk = (PIMAGE_THUNK_DATA)((BYTE*)hModule + import_desc->FirstThunk);

            while (thunk->u1.AddressOfData) {
                if (thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) continue;

                PIMAGE_IMPORT_BY_NAME import = (PIMAGE_IMPORT_BY_NAME)((BYTE*)hModule + thunk->u1.AddressOfData);
                if (strcmp(import->Name, func_name) == 0) {
                    DWORD old_protect;
                    VirtualProtect(&thunk->u1.Function, sizeof(ULONG_PTR), PAGE_READWRITE, &old_protect);

                    if (old_func) *old_func = (void*)thunk->u1.Function;
                    thunk->u1.Function = (ULONGLONG)new_func;

                    VirtualProtect(&thunk->u1.Function, sizeof(ULONG_PTR), old_protect, &old_protect);

                    BDM_Log(BDM_LOG_LEVEL_INFO, "HookManager: IAT Hook %s!%s", module_name, func_name);
                    return 0;
                }
                thunk++;
            }
        }
        import_desc++;
    }

    return -1;
}

LRESULT HookManager::CallNextHook(int hook_id, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    HookEntry* entry = FindHook(hook_id);
    if (!entry || !entry->active) return 0;

    if (entry->type == BDM_HOOK_WNDPROC && entry->callback) {
        HookProc proc = (HookProc)entry->callback;
        proc(hwnd, msg, wParam, lParam, entry->user_data);
    }

    return 0;
}

void HookManager::RemoveAllHooks() {
    EnterCriticalSection(&m_cs);

    if (m_wndProcHookInstalled && m_targetHwnd && m_originalWndProc) {
        SetWindowLongPtr(m_targetHwnd, GWLP_WNDPROC, (LONG_PTR)m_originalWndProc);
        m_wndProcHookInstalled = FALSE;
    }

    for (auto hook : m_winEventHooks) {
        UnhookWinEvent(hook);
    }
    m_winEventHooks.clear();

    for (auto entry : m_hooks) {
        delete entry;
    }
    m_hooks.clear();

    LeaveCriticalSection(&m_cs);
}

LRESULT CALLBACK HookManager::WndProcHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code < 0) return CallNextHook(0, NULL, 0, 0, 0);

    HookManager& mgr = HookManager::Instance();
    HWND hwnd = (HWND)wParam;
    UINT msg = 0;
    WPARAM wParam_hook = 0;
    LPARAM lParam_hook = 0;

    if (code == HC_ACTION) {
        CWPSTRUCT* cwp = (CWPSTRUCT*)lParam;
        hwnd = cwp->hwnd;
        msg = cwp->message;
        wParam_hook = cwp->wParam;
        lParam_hook = cwp->lParam;
    }

    EnterCriticalSection(&mgr.m_cs);
    std::list<HookEntry*> wndproc_hooks;
    for (auto entry : mgr.m_hooks) {
        if (entry->type == BDM_HOOK_WNDPROC && entry->active) {
            wndproc_hooks.push_back(entry);
        }
    }
    LeaveCriticalSection(&mgr.m_cs);

    for (auto entry : wndproc_hooks) {
        if (entry->callback) {
            HookProc proc = (HookProc)entry->callback;
            proc(hwnd, msg, wParam_hook, lParam_hook, entry->user_data);
        }
    }

    return CallNextHook(0, hwnd, msg, wParam_hook, lParam_hook);
}

void CALLBACK HookManager::WinEventProc(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG idObject, LONG idChild,
    DWORD dwEventThread, DWORD dwmsEventTime) {

    if (idObject != OBJID_WINDOW || idChild != CHILDID_SELF) return;

    HookManager& mgr = HookManager::Instance();

    EnterCriticalSection(&mgr.m_cs);
    std::list<HookEntry*> event_hooks;
    for (auto entry : mgr.m_hooks) {
        if (entry->type == BDM_HOOK_SETWINEVENT && entry->active) {
            event_hooks.push_back(entry);
        }
    }
    LeaveCriticalSection(&mgr.m_cs);

    for (auto entry : event_hooks) {
        if (entry->callback) {
            HookProc proc = (HookProc)entry->callback;
            proc(hwnd, event, 0, 0, entry->user_data);
        }
    }
}
