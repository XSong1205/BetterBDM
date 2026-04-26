# BetterBDM - 波点音乐 Windows 客户端 功能增强


## 项目结构

```
BetterBDM/
├── injector/              # 注入器 (injector.exe)
├── core/                  # 核心 DLL (core.dll)
├── plugins/               # 插件目录
│   └── media_control/     # 媒体控制插件示例
├── include/               # 共享头文件
├── build/                 # 构建输出
├── SPEC.md                # 设计规格文档
└── build.cmd              # 构建脚本
```


### 3. 插件开发

在 `plugins/` 目录下创建新插件：

```
plugins/
├── my_plugin/
│   ├── plugin.json       # 插件描述文件
│   └── plugin.cpp        # 插件源码
```

## 插件 API

```cpp
struct PluginInterface {
    int api_version;

    // 生命周期
    Plugin_OnLoad on_load;
    Plugin_OnUnload on_unload;
    Plugin_OnEnable on_enable;
    Plugin_OnDisable on_disable;

    // Hook 注册
    int (*register_hook)(const char* type, HookCallback callback, void* user_data, int priority);
    int (*unregister_hook)(int hook_id);

    // Flutter 桥接
    int (*call_flutter_method)(const char* method, const char* args, char** result);

    // 日志
    void (*log)(int level, const char* format, ...);

    // 内存管理
    void* (*malloc)(size_t size);
    void (*free)(void* ptr);

    void* user_data;
};
```

## Hook 类型

- `WM_APPCOMMAND` - 媒体键
- `WndProc` - 窗口过程
- `IAT Hook` - 导入表 Hook
- `WinEvent` - 窗口事件

## 技术实现

- DLL 注入: CreateRemoteThread + LoadLibrary
- Hook: SetWindowLongPtr (GWLP_WNDPROC)
- Flutter 桥接: WM_COPYDATA / 窗口子类化
