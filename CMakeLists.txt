cmake_minimum_required(VERSION 3.20)
project(GuessDraw)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mwindows")

add_executable(GuessDraw main.cpp)

target_link_libraries(GuessDraw gdiplus comctl32 shell32 ole32)


#[[
窗口可以自由拖动   ctrl+鼠标

可以动态切换图片(指定路径下最新时间的)

隐藏窗口和图片，方便查看实际效果

调整图片 透明度，黑白化，去白色底

]]