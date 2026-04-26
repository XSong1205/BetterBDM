cmake_minimum_required(VERSION 3.16)
project(BetterBDM VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

if(MSVC)
    add_compile_options(/W4 /WX- /wd4996)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra)
endif()

include_directories(${CMAKE_SOURCE_DIR}/include)

set(INJECTOR_SOURCES
    injector/injector.cpp
    injector/injector.h
    injector/process_utils.cpp
    injector/process_utils.h
    injector/pe_utils.cpp
    injector/pe_utils.h
)

set(CORE_SOURCES
    core/core.cpp
    core/core.h
    core/hook_manager.cpp
    core/hook_manager.h
    core/plugin_manager.cpp
    core/plugin_manager.h
    core/flutter_bridge.cpp
    core/flutter_bridge.h
)

set(PLUGIN_SOURCES
    plugins/media_control/plugin.cpp
)

add_executable(injector WIN32 ${INJECTOR_SOURCES})
target_link_libraries(injector winhttp kernel32 psapi)
set_target_properties(injector PROPERTIES OUTPUT_NAME "BetterBDM-Injector")

add_library(core SHARED ${CORE_SOURCES})
target_link_libraries(core winmm user32)
set_target_properties(core PROPERTIES OUTPUT_NAME "BetterBDM-Core")

add_library(media_control SHARED ${PLUGIN_SOURCES})
target_include_directories(media_control PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(media_control)
set_target_properties(media_control PROPERTIES OUTPUT_NAME "BDM-Plugin-MediaControl")

set_target_properties(core media_control PROPERTIES
    PREFIX ""
    IMPORT_PREFIX ""
)

set_target_properties(core media_control PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/build"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/build"
)

install(TARGETS injector core media_control
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION bin
)

message(STATUS "BetterBDM Build Configuration:")
message(STATUS "  Injector: ${CMAKE_BINARY_DIR}/bin/injector.exe")
message(STATUS "  Core DLL: ${CMAKE_BINARY_DIR}/bin/core.dll")
message(STATUS "  Plugin:   ${CMAKE_BINARY_DIR}/bin/BDM-Plugin-MediaControl.dll")
