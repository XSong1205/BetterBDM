# BetterBDM - Windows 桌面应用注入框架

## 1. 项目概述

**项目名称**: BetterBDM (Better Desktop Mod)
**类型**: Windows DLL 注入框架 + 插件系统
**核心功能**: 通过 DLL 注入方式为 Flutter Windows 桌面程序（波点音乐）提供功能扩展
**目标程序**: 波点音乐 Flutter Desktop (flutter_assets 目录结构)

## 2. 系统架构

```
BetterBDM/
├── injector/                    # 注入器（独立 EXE）
│   ├── injector.cpp            # 主注入程序
│   ├── pe_utils.cpp            # PE 工具函数
│   ├── pe_utils.h
│   ├── process_utils.cpp       # 进程操作
│   ├── process_utils.h
│   └── injector.h
├── core/                        # 核心 DLL（被注入的 DLL）
│   ├── core.cpp                # 核心入口
│   ├── core.h
│   ├── hook_manager.cpp        # Hook 管理器
│   ├── hook_manager.h
│   ├── plugin_manager.cpp      # 插件管理器
│   ├── plugin_manager.h
│   ├── flutter_bridge.cpp      # Flutter 桥接
│   └── flutter_bridge.h
├── plugins/                     # 插件目录
│   ├── plugin.json             # 插件注册表
│   └── media_control/          # 媒体控制插件示例
│       ├── plugin.json
│       └── plugin.cpp
├── include/                     # 共享头文件
│   ├── bdmicros.h              # 框架核心类型定义
│   └── plugin_api.h            # 插件 API
├── build/                       # 构建输出
└── docs/                        # 文档
```

## 3. 核心组件设计

### 3.1 注入器 (injector.exe)

**功能**:
- 自动定位目标进程（按窗口标题 "波点音乐" 或进程名）
- 获取目标进程的 LoadLibrary 地址（kernel32.dll）
- 使用 CreateRemoteThread + LoadLibrary 注入 core.dll
- 注入前备份目标进程内存状态（可选）
- 传递插件目录路径给 core.dll

**注入流程**:
1. FindWindow / EnumProcesses 定位目标
2. OpenProcess 打开目标进程
3. VirtualAllocEx 分配内存
4. WriteProcessMemory 写入 DLL 路径
5. CreateRemoteThread(LoadLibrary)
6. WaitForSingleObject 等待加载完成

### 3.2 核心 DLL (core.dll)

**入口点**:
- DllMain: 初始化/清理
- Core_Initialize: 插件系统初始化
- Core_Uninitialize: 插件卸载

**核心模块**:

#### HookManager
- 支持 Hook 类型:
  - WndProc (SetWindowsHookEx / SetWindowLongPtr)
  - Win32 API Hook (IAT Hook / Inline Hook)
  - 消息循环 Hook (SetWinEventHook)
- Hook 链管理（多个插件可注册同一 Hook）
- Hook 优先级机制

#### PluginManager
- 扫描 plugins/ 目录
- 解析 plugin.json 描述文件
- 动态加载插件 DLL
- 生命周期管理 (onLoad, onUnload, onEnable, onDisable)
- 插件依赖解析

#### FlutterBridge
- FlutterEngine 地址获取（通过窗口类名或进程扫描）
- MethodChannel 调用封装
- 事件通道监听
- Flutter Engine 消息循环 Hook

### 3.3 插件系统

**插件结构**:
```
plugins/
├── plugin.json          # 全局插件注册表
└── <plugin_name>/
    ├── plugin.json      # 插件描述
    └── plugin.dll       # 插件 DLL
```

**plugin.json 格式**:
```json
{
  "name": "media_control",
  "version": "1.0.0",
  "author": "BetterBDM",
  "description": "Hook 媒体控制键",
  "main": "plugin.dll",
  "dependencies": [],
  "hooks": ["WM_APPCOMMAND", "SetWindowLongPtr"],
  "api_version": "1.0.0"
}
```

**插件 API (plugin_api.h)**:
```cpp
// 生命周期
typedef bool (*Plugin_OnLoad)(void* env);
typedef void (*Plugin_OnUnload)();
typedef void (*Plugin_OnEnable)();
typedef void (*Plugin_OnDisable)();

// Hook 注册
typedef int (*RegisterHook)(HookInfo* info);
typedef int (*UnregisterHook)(int hook_id);

// Flutter 桥接
typedef int (*CallFlutterMethod)(const char* method, const char* args, void** result);
typedef int (*ListenFlutterEvent)(const char* channel, EventCallback callback);

// 日志
typedef void (*Log)(int level, const char* format, ...);
```

**插件生命周期**:
1. 注入后 core.dll 扫描 plugins/ 目录
2. 加载 plugin.json 验证
3. 动态加载插件 DLL
4. 调用 OnLoad 初始化
5. 注册 Hook 点
6. OnEnable 激活插件
7. 运行时可动态禁用/启用
8. 退出时调用 OnUnload

## 4. 功能清单

### 4.1 注入器功能
- [x] 进程自动定位（窗口标题/进程名）
- [x] DLL 注入（CreateRemoteThread + LoadLibrary）
- [x] 远程线程执行回调
- [x] 注入状态检测
- [x] 进程信息显示

### 4.2 核心 DLL 功能
- [x] 插件系统初始化
- [x] 插件目录扫描
- [x] 插件动态加载/卸载
- [x] Hook 管理（注册/注销/链式调用）
- [x] WndProc Hook (WM_APPCOMMAND)
- [x] Flutter MethodChannel 桥接
- [x] 日志系统

### 4.3 插件示例 - 媒体控制
- [x] Hook WM_APPCOMMAND 消息
- [x] 捕获媒体键（播放/暂停/上一首/下一首）
- [x] 调用 Flutter MethodChannel 控制播放
- [x] 显示通知

### 4.4 扩展功能（预留）
- [ ] 窗口管理 Hook
- [ ] Flutter Engine 直接访问
- [ ] 插件热更新
- [ ] 插件管理 UI

## 5. 技术细节

### 5.1 关键技术点

**DLL 注入**:
- CreateRemoteThread with LoadLibrary
- 获取 kernel32.dll!LoadLibraryW 地址
- 宽字符路径支持

**Hook 技术**:
- SetWindowLongPtr / GWLP_WNDPROC for WndProc
- IAT Hook for imported functions
- Trampoline 跳转（5字节 jmp）

**Flutter 桥接**:
- 定位 FlutterEngine（窗口类名 "FlutterView" 或内存扫描）
- MethodChannel 序列化/反序列化
- BinaryMessenger 调用

### 5.2 编译要求
- MSVC 2019+ / MinGW-w64
- Windows SDK 10.0.19041.0+
- CMake 3.16+ 或直接 Makefile

### 5.3 目标平台
- Windows 10/11 x64
- Flutter Windows Desktop

## 6. 安全与限制

- 仅用于教育/研究目的
- 禁止用于商业违规用途
- 依赖 Windows API 兼容性
