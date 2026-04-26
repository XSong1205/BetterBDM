#pragma once

#include "..\include\bdmicros.h"
#include <windows.h>
#include <list>

typedef struct {
    const char* channel;
    void (*callback)(const char* event, void* data);
    void* user_data;
} EventListener;

class FlutterBridge {
public:
    static FlutterBridge& Instance();

    BDM_RESULT Initialize(HWND target_hwnd, void* engine_ptr);
    void Uninitialize();

    BDM_RESULT CallMethod(const char* method, const char* args, char** result);
    BDM_RESULT ListenEvent(const char* channel, void (*callback)(const char*, void*), void* user_data);
    BDM_RESULT UnlistenEvent(const char* channel);

    void* GetEngine() const { return m_enginePtr; }
    HWND GetTargetHwnd() const { return m_targetHwnd; }

private:
    FlutterBridge();
    ~FlutterBridge();

    HWND FindFlutterWindow();
    void* FindFlutterEngine();
    BOOL SetupMethodChannel();

    static LRESULT CALLBACK SubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND m_targetHwnd;
    HWND m_flutterHwnd;
    void* m_enginePtr;
    WNDPROC m_originalWndProc;
    BOOL m_initialized;

    std::list<EventListener*> m_listeners;
    CRITICAL_SECTION m_cs;
};
