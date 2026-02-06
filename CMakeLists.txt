cmake_minimum_required(VERSION 3.20)
project(GuessDraw)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mwindows")
add_definitions(-DUNICODE -D_UNICODE)

# MinGW UTF-8 源码编码支持
if(MINGW)
    add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

include_directories(src/core src/ui)

add_executable(GuessDraw
        src/main.cpp
        src/core/drawing.cpp
        src/core/config.cpp
        src/ui/tray.cpp
        src/ui/settings.cpp
        src/ui/hotkeys.cpp
        res/app.rc
)

target_link_libraries(GuessDraw gdiplus comctl32 shell32 ole32)