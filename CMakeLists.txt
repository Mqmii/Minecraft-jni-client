cmake_minimum_required(VERSION 3.20)
project(MinecraftWrapperJNI)

set(CMAKE_CXX_STANDARD 20)

find_package(JNI REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static -static-libgcc -static-libstdc++")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")

add_compile_definitions(UNICODE _UNICODE)

set(MINHOOK_SOURCES
        minhook/src/buffer.c
        minhook/src/hook.c
        minhook/src/trampoline.c
        minhook/src/hde/hde32.c
        minhook/src/hde/hde64.c
)

set(IMGUI_SOURCES
        imgui/imgui.cpp
        imgui/imgui_draw.cpp
        imgui/imgui_tables.cpp
        imgui/imgui_widgets.cpp
        imgui/backends/imgui_impl_win32.cpp
        imgui/backends/imgui_impl_opengl3.cpp
        modules/Esp.cpp
        modules/Esp.hpp
)

add_library(MinecraftWrapperJNI SHARED
        CrashDiagnostics.cpp
        main.cpp
        jni/JniEnvironment.cpp
        ui/ImGuiMenu.cpp
        ui/EspOverlayRenderer.cpp
        ui/UiHookManager.cpp
        classes/minecraft.cpp
        modules/FastBreak.cpp
        modules/FastPlace.cpp
        modules/Velocity.cpp
        modules/click.cpp
        modules/hitResult.cpp
        classes/Entity.cpp
        ${IMGUI_SOURCES}
        ${MINHOOK_SOURCES}
)

target_include_directories(MinecraftWrapperJNI PRIVATE
        ${JNI_INCLUDE_DIRS}
        glm
        imgui
        imgui/backends
        minhook/include
)

if(WIN32)
    target_link_libraries(MinecraftWrapperJNI PRIVATE kernel32 user32 opengl32 gdi32 dwmapi dbghelp)
endif()
