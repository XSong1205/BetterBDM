#pragma once

#include <windows.h>

typedef struct {
    PIMAGE_DOS_HEADER dos_header;
    PIMAGE_NT_HEADERS nt_headers;
    PIMAGE_OPTIONAL_HEADER64 optional_header;
    PIMAGE_SECTION_HEADER* sections;
    DWORD num_sections;
} PE_Info;

BOOL PE_LoadFromMemory(LPCVOID module_base, PE_Info* info);
BOOL PE_GetExportFunction(LPCVOID module_base, const char* func_name, PULONGLONG out_addr);
BOOL PE_IsValidPE(LPCVOID module_base);
void PE_Free(PE_Info* info);

HMODULE PE_GetLoadedModule(HANDLE hProcess, HMODULE module_base);
BOOL PE_EnumerateLoadedModules(HANDLE hProcess, void (*callback)(HMODULE module, const char* name, void* user_data), void* user_data);
