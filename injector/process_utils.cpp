#include "process_utils.h"
#include "pe_utils.h"
#include <tlhelp32.h>
#include <string.h>

BOOL Process_FindByWindowTitle(const wchar_t* title, ProcessInfo* info) {
    if (!title || !info) return FALSE;

    memset(info, 0, sizeof(ProcessInfo));

    HWND hwnd = FindWindowW(NULL, title);
    if (!hwnd) {
        hwnd = FindWindowW(L"FlutterView", title);
    }

    if (!hwnd) {
        DWORD pid = 0;
        hwnd = FindWindowExW(NULL, NULL, L"FlutterView", title);
        if (!hwnd) {
            return FALSE;
        }
    }

    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);

    info->hwnd = hwnd;
    info->pid = pid;

    GetWindowTextA(hwnd, info->window_title, sizeof(info->window_title));

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess) {
        GetModuleBaseNameA(hProcess, NULL, info->process_name, sizeof(info->process_name));
        info->hProcess = hProcess;
    }

    return TRUE;
}

BOOL Process_FindByName(const char* name, ProcessInfo* info) {
    if (!name || !info) return FALSE;

    memset(info, 0, sizeof(ProcessInfo));

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return FALSE;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            if (StrStrIW(pe.szExeFile, L".exe")) {
                char exe_name[MAX_PATH];
                WideCharToMultiByte(CP_ACP, 0, pe.szExeFile, -1, exe_name, sizeof(exe_name), NULL, NULL);

                if (strstr(exe_name, name)) {
                    info->pid = pe.th32ProcessID;
                    strncpy(info->process_name, exe_name, sizeof(info->process_name) - 1);

                    info->hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_CREATE_THREAD, FALSE, info->pid);
                    if (info->hProcess) {
                        HWND hwnd = NULL;
                        DWORD threadId = 0;
                        HANDLE hThreadSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
                        if (hThreadSnapshot != INVALID_HANDLE_VALUE) {
                            THREADENTRY32 te;
                            te.dwSize = sizeof(THREADENTRY32);
                            if (Thread32First(hThreadSnapshot, &te)) {
                                do {
                                    if (te.th32OwnerProcessID == info->pid) {
                                        threadId = te.th32ThreadID;
                                        break;
                                    }
                                } while (Thread32Next(hThreadSnapshot, &te));
                            }
                            CloseHandle(hThreadSnapshot);
                        }

                        if (threadId) {
                            HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, threadId);
                            if (hThread) {
                                AttachThreadInput(GetCurrentThreadId(), threadId, TRUE);
                                EnumThreadWindows(threadId, [](HWND hwnd, LPARAM lParam) -> BOOL {
                                    ProcessInfo* info = (ProcessInfo*)lParam;
                                    info->hwnd = hwnd;
                                    char title[256];
                                    if (GetWindowTextA(hwnd, title, sizeof(title))) {
                                        strncpy(info->window_title, title, sizeof(info->window_title) - 1);
                                    }
                                    return FALSE;
                                }, (LPARAM)info);
                                AttachThreadInput(GetCurrentThreadId(), threadId, FALSE);
                                CloseHandle(hThread);
                            }
                        }
                    }

                    CloseHandle(hSnapshot);
                    return TRUE;
                }
            }
        } while (Process32NextW(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
    return FALSE;
}

BOOL Process_Open(DWORD pid, ProcessInfo* info) {
    if (!info) return FALSE;

    memset(info, 0, sizeof(ProcessInfo));

    info->pid = pid;
    info->hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_CREATE_THREAD, FALSE, pid);

    if (!info->hProcess) return FALSE;

    GetModuleBaseNameA(info->hProcess, NULL, info->process_name, sizeof(info->process_name));

    HANDLE hThreadSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hThreadSnapshot != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te;
        te.dwSize = sizeof(THREADENTRY32);
        if (Thread32First(hThreadSnapshot, &te)) {
            do {
                if (te.th32OwnerProcessID == pid) {
                    HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, te.th32ThreadID);
                    if (hThread) {
                        AttachThreadInput(GetCurrentThreadId(), te.th32ThreadID, TRUE);
                        EnumThreadWindows(te.th32ThreadID, [](HWND hwnd, LPARAM lParam) -> BOOL {
                            ProcessInfo* info = (ProcessInfo*)lParam;
                            info->hwnd = hwnd;
                            char title[256];
                            if (GetWindowTextA(hwnd, title, sizeof(title))) {
                                strncpy(info->window_title, title, sizeof(info->window_title) - 1);
                            }
                            return FALSE;
                        }, (LPARAM)info);
                        AttachThreadInput(GetCurrentThreadId(), te.th32ThreadID, FALSE);
                        CloseHandle(hThread);
                    }
                    break;
                }
            } while (Thread32Next(hThreadSnapshot, &te));
        }
        CloseHandle(hThreadSnapshot);
    }

    return TRUE;
}

void Process_Close(ProcessInfo* info) {
    if (info && info->hProcess) {
        CloseHandle(info->hProcess);
        info->hProcess = NULL;
    }
}

typedef struct _INJECTION_PARAM {
    wchar_t dll_path[MAX_PATH];
} INJECTION_PARAM;

static DWORD __stdcall InjectionThreadProc(LPVOID param) {
    INJECTION_PARAM* p = (INJECTION_PARAM*)param;
    HMODULE hModule = LoadLibraryW(p->dll_path);
    (void)hModule;
    return 0;
}

BOOL Process_InjectDLL(ProcessInfo* info, const wchar_t* dll_path) {
    if (!info || !info->hProcess || !dll_path) return FALSE;

    SIZE_T param_size = sizeof(INJECTION_PARAM);
    LPVOID remote_param = VirtualAllocEx(info->hProcess, NULL, param_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote_param) return FALSE;

    INJECTION_PARAM param;
    wcsncpy(param.dll_path, dll_path, MAX_PATH - 1);
    param.dll_path[MAX_PATH - 1] = L'\0';

    if (!WriteProcessMemory(info->hProcess, remote_param, &param, sizeof(param), NULL)) {
        VirtualFreeEx(info->hProcess, remote_param, 0, MEM_RELEASE);
        return FALSE;
    }

    HMODULE hKernel = GetModuleHandleW(L"kernel32.dll");
    LPVOID loadlib_addr = (LPVOID)GetProcAddress(hKernel, "LoadLibraryW");

    HANDLE hThread = CreateRemoteThread(info->hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)loadlib_addr, remote_param, 0, NULL);
    if (!hThread) {
        VirtualFreeEx(info->hProcess, remote_param, 0, MEM_RELEASE);
        return FALSE;
    }

    WaitForSingleObject(hThread, INFINITE);
    DWORD exit_code = 0;
    GetExitCodeThread(hThread, &exit_code);

    CloseHandle(hThread);
    VirtualFreeEx(info->hProcess, remote_param, 0, MEM_RELEASE);

    return (exit_code != 0);
}

BOOL Process_EjectDLL(ProcessInfo* info, const wchar_t* dll_name) {
    if (!info || !info->hProcess || !dll_name) return FALSE;

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, info->pid);
    if (hSnapshot == INVALID_HANDLE_VALUE) return FALSE;

    BOOL found = FALSE;
    MODULEENTRY32W me;
    me.dwSize = sizeof(MODULEENTRY32W);

    if (Module32FirstW(hSnapshot, &me)) {
        do {
            if (wcsstr(me.szModule, dll_name) || wcsstr(me.szExePath, dll_name)) {
                HANDLE hThread = CreateRemoteThread(info->hProcess, NULL, 0,
                    (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "FreeLibrary"),
                    me.hModule, 0, NULL);
                if (hThread) {
                    WaitForSingleObject(hThread, INFINITE);
                    CloseHandle(hThread);
                    found = TRUE;
                }
                break;
            }
        } while (Module32NextW(hSnapshot, &me));
    }

    CloseHandle(hSnapshot);
    return found;
}

BOOL Process_IsLoaded(ProcessInfo* info, const wchar_t* dll_name) {
    if (!info || !info->hProcess || !dll_name) return FALSE;

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, info->pid);
    if (hSnapshot == INVALID_HANDLE_VALUE) return FALSE;

    BOOL found = FALSE;
    MODULEENTRY32W me;
    me.dwSize = sizeof(MODULEENTRY32W);

    if (Module32FirstW(hSnapshot, &me)) {
        do {
            if (wcsstr(me.szModule, dll_name)) {
                found = TRUE;
                break;
            }
        } while (Module32NextW(hSnapshot, &me));
    }

    CloseHandle(hSnapshot);
    return found;
}

BOOL Process_AllocateRemote(ProcessInfo* info, LPVOID* out_addr, SIZE_T size) {
    if (!info || !info->hProcess || !out_addr) return FALSE;

    *out_addr = VirtualAllocEx(info->hProcess, NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    return (*out_addr != NULL);
}

BOOL Process_FreeRemote(ProcessInfo* info, LPVOID addr) {
    if (!info || !info->hProcess || !addr) return FALSE;

    return VirtualFreeEx(info->hProcess, addr, 0, MEM_RELEASE);
}

BOOL Process_WriteMemory(ProcessInfo* info, LPVOID addr, LPCVOID data, SIZE_T size) {
    if (!info || !info->hProcess || !addr || !data) return FALSE;

    return WriteProcessMemory(info->hProcess, addr, data, size, NULL);
}

BOOL Process_ReadMemory(ProcessInfo* info, LPCVOID addr, LPVOID data, SIZE_T size) {
    if (!info || !info->hProcess || !addr || !data) return FALSE;

    SIZE_T bytes_read = 0;
    return ReadProcessMemory(info->hProcess, addr, data, size, &bytes_read) && bytes_read == size;
}

HMODULE Injector_GetModuleBase(DWORD pid, const char* module_name) {
    if (!module_name) return NULL;

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (hSnapshot == INVALID_HANDLE_VALUE) return NULL;

    MODULEENTRY32W me;
    me.dwSize = sizeof(MODULEENTRY32W);

    if (Module32FirstW(hSnapshot, &me)) {
        do {
            char name[MAX_PATH];
            WideCharToMultiByte(CP_ACP, 0, me.szModule, -1, name, sizeof(name), NULL, NULL);
            if (strstr(name, module_name)) {
                CloseHandle(hSnapshot);
                return me.hModule;
            }
        } while (Module32NextW(hSnapshot, &me));
    }

    CloseHandle(hSnapshot);
    return NULL;
}
