#pragma once

#include <windows.h>

typedef struct {
    DWORD pid;
    HWND hwnd;
    char window_title[256];
    char process_name[256];
    HANDLE hProcess;
} ProcessInfo;

BOOL Process_FindByWindowTitle(const wchar_t* title, ProcessInfo* info);
BOOL Process_FindByName(const char* name, ProcessInfo* info);
BOOL Process_Open(DWORD pid, ProcessInfo* info);
void Process_Close(ProcessInfo* info);

BOOL Process_InjectDLL(ProcessInfo* info, const wchar_t* dll_path);
BOOL Process_EjectDLL(ProcessInfo* info, const wchar_t* dll_path);

BOOL Process_IsLoaded(ProcessInfo* info, const wchar_t* dll_name);

BOOL Process_AllocateRemote(ProcessInfo* info, LPVOID* out_addr, SIZE_T size);
BOOL Process_FreeRemote(ProcessInfo* info, LPVOID addr);
BOOL Process_WriteMemory(ProcessInfo* info, LPVOID addr, LPCVOID data, SIZE_T size);
BOOL Process_ReadMemory(ProcessInfo* info, LPCVOID addr, LPVOID data, SIZE_T size);

HMODULE Injector_GetModuleBase(DWORD pid, const char* module_name);
