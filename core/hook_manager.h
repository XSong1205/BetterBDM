#pragma once

#include "..\include\bdmicros.h"
#include <windows.h>
#include <list>

typedef struct {
    int hook_id;
    BDM_HOOK_TYPE type;
    const char* name;
    void* callback;
    void* user_data;
    int priority;
    BOOL active;
    void* original_addr;
    BYTE original_bytes[16];
    BYTE trampoline[16];
} HookEntry;

typedef void (*HookProc)(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, void* user_data);

class HookManager {
public:
    static HookManager& Instance();

    int RegisterWndProcHook(HWND hwnd, HookProc callback, void* user_data, int priority = 0);
    BOOL UnregisterHook(int hook_id);
    BOOL EnableHook(int hook_id);
    BOOL DisableHook(int hook_id);

    int RegisterWinEventHook(DWORD event_min, DWORD event_max, HookProc callback, void* user_data, int priority = 0);
    int RegisterIATHook(const char* module_name, const char* func_name, void* new_func, void** old_func);

    LRESULT CallNextHook(int hook_id, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void RemoveAllHooks();

private:
    HookManager();
    ~HookManager();

    int GenerateHookId();

    int RegisterHook(HookEntry* entry);

    static LRESULT CALLBACK WndProcHookProc(int code, WPARAM wParam, LPARAM lParam);
    static void CALLBACK WinEventProc(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG idObject, LONG idChild,
        DWORD dwEventThread, DWORD dwmsEventTime);

    HookEntry* FindHook(int hook_id);
    HookEntry* FindHookByHwnd(HWND hwnd);

    int FindNextPriority(BDM_HOOK_TYPE type, HWND hwnd = NULL);

    std::list<HookEntry*> m_hooks;
    std::list<HWINEVENTHOOK> m_winEventHooks;
    int m_nextHookId;
    BOOL m_wndProcHookInstalled;
    HWND m_targetHwnd;
    WNDPROC m_originalWndProc;

    CRITICAL_SECTION m_cs;
};

#define WM_APPCOMMAND 0x0318
#define APPCOMMAND_MEDIA_PLAY_PAUSE                14
#define APPCOMMAND_MEDIA_STOP                      13
#define APPCOMMAND_MEDIA_PREVIOUS_TRACK            12
#define APPCOMMAND_MEDIA_NEXT_TRACK                11
#define APPCOMMAND_VOLUME_MUTE                     8
#define APPCOMMAND_VOLUME_UP                       9
#define APPCOMMAND_VOLUME_DOWN                     10
#define APPCOMMAND_MEDIA_PLAY                      46
#define APPCOMMAND_MEDIA_PAUSE                      47
#define APPCOMMAND_MEDIA_RECORD                    48
#define APPCOMMAND_MEDIA_FAST_FORWARD              49
#define APPCOMMAND_MEDIA_REWIND                    50
#define APPCOMMAND_MEDIA_CHANNEL_UP                51
#define APPCOMMAND_MEDIA_CHANNEL_DOWN              52
