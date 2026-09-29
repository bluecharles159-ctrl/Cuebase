cmake_minimum_required(VERSION 3.20)
project(Cuebase VERSION 0.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia)

add_executable(Cuebase WIN32
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/HelpCenter.h
    src/HelpCenter.cpp
    src/AppLinks.h
    src/Data.h
    src/Data.cpp
    src/Playout.h
    src/Playout.cpp
    src/ViewportWidget.h
    src/ViewportWidget.cpp
    src/TimelineWidget.h
    src/TimelineWidget.cpp
    src/QueueStrip.h
    src/QueueStrip.cpp
    src/EditorPanel.h
    src/EditorPanel.cpp
    src/ControlPanel.h
    src/ControlPanel.cpp
)

target_link_libraries(Cuebase PRIVATE Qt6::Widgets Qt6::Multimedia)

target_compile_definitions(Cuebase PRIVATE "CUEBASE_VERSION=\"${PROJECT_VERSION}\"")
