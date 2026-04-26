#include "pe_utils.h"
#include <intrin.h>

BOOL PE_IsValidPE(LPCVOID module_base) {
    if (!module_base) return FALSE;

    PIMAGE_DOS_HEADER dos_header = (PIMAGE_DOS_HEADER)module_base;
    if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) return FALSE;

    PIMAGE_NT_HEADERS nt_headers = (PIMAGE_NT_HEADERS)((BYTE*)module_base + dos_header->e_lfanew);
    if (nt_headers->Signature != IMAGE_NT_SIGNATURE) return FALSE;

    return TRUE;
}

BOOL PE_LoadFromMemory(LPCVOID module_base, PE_Info* info) {
    if (!PE_IsValidPE(module_base)) return FALSE;

    memset(info, 0, sizeof(PE_Info));

    PIMAGE_DOS_HEADER dos_header = (PIMAGE_DOS_HEADER)module_base;
    PIMAGE_NT_HEADERS nt_headers = (PIMAGE_NT_HEADERS)((BYTE*)module_base + dos_header->e_lfanew);

    info->dos_header = dos_header;
    info->nt_headers = nt_headers;
    info->optional_header = &nt_headers->OptionalHeader;
    info->num_sections = nt_headers->FileHeader.NumberOfSections;

    if (info->num_sections > 0) {
        info->sections = (PIMAGE_SECTION_HEADER*)malloc(sizeof(PIMAGE_SECTION_HEADER) * info->num_sections);
        if (!info->sections) return FALSE;

        PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(nt_headers);
        for (DWORD i = 0; i < info->num_sections; i++) {
            info->sections[i] = section++;
        }
    }

    return TRUE;
}

BOOL PE_GetExportFunction(LPCVOID module_base, const char* func_name, PULONGLONG out_addr) {
    if (!module_base || !func_name || !out_addr) return FALSE;

    PIMAGE_DOS_HEADER dos_header = (PIMAGE_DOS_HEADER)module_base;
    PIMAGE_NT_HEADERS nt_headers = (PIMAGE_NT_HEADERS)((BYTE*)module_base + dos_header->e_lfanew);
    PIMAGE_DATA_DIRECTORY export_dir = &nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];

    if (export_dir->Size == 0) return FALSE;

    PIMAGE_EXPORT_DIRECTORY export_table = (PIMAGE_EXPORT_DIRECTORY)((BYTE*)module_base + export_dir->VirtualAddress);

    DWORD* name_refs = (DWORD*)((BYTE*)module_base + export_table->AddressOfNames);
    WORD* ordinals = (WORD*)((BYTE*)module_base + export_table->AddressOfNameOrdinals);
    DWORD* functions = (DWORD*)((BYTE*)module_base + export_table->AddressOfFunctions);

    for (DWORD i = 0; i < export_table->NumberOfNames; i++) {
        const char* export_name = (const char*)((BYTE*)module_base + name_refs[i]);
        if (strcmp(export_name, func_name) == 0) {
            DWORD func_rva = functions[ordinals[i]];
            *out_addr = (ULONGLONG)((BYTE*)module_base + func_rva);
            return TRUE;
        }
    }

    return FALSE;
}

void PE_Free(PE_Info* info) {
    if (info->sections) {
        free(info->sections);
        info->sections = NULL;
    }
    memset(info, 0, sizeof(PE_Info));
}

HMODULE PE_GetLoadedModule(HANDLE hProcess, HMODULE module_base) {
    if (!hProcess || !module_base) return NULL;

    BYTE header[4096];
    SIZE_T bytes_read;

    if (!ReadProcessMemory(hProcess, module_base, header, sizeof(header), &bytes_read)) {
        return NULL;
    }

    if (!PE_IsValidPE(header)) return NULL;

    return module_base;
}

BOOL PE_EnumerateLoadedModules(HANDLE hProcess, void (*callback)(HMODULE module, const char* name, void* user_data), void* user_data) {
    if (!hProcess || !callback) return FALSE;

    DWORD bytes_needed = 0;
    if (!EnumProcessModules(hProcess, NULL, 0, &bytes_needed)) {
        return FALSE;
    }

    DWORD num_modules = bytes_needed / sizeof(HMODULE);
    HMODULE* modules = (HMODULE*)malloc(bytes_needed);
    if (!modules) return FALSE;

    if (!EnumProcessModules(hProcess, modules, bytes_needed, &bytes_needed)) {
        free(modules);
        return FALSE;
    }

    char module_name[MAX_PATH];
    for (DWORD i = 0; i < num_modules; i++) {
        if (GetModuleBaseNameA(hProcess, modules[i], module_name, sizeof(module_name))) {
            callback(modules[i], module_name, user_data);
        }
    }

    free(modules);
    return TRUE;
}
