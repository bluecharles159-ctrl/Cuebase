cmake_minimum_required(VERSION 3.20)
project(Cuebase VERSION 0.5.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Multimedia Network Svg)
find_package(assimp CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(AV REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswscale)

# The application and regression suite share the editor and renderer objects.
add_library(CuebaseBuildCore STATIC
    src/Data.h src/Data.cpp
    src/ElementLibrary.h src/ElementLibrary.cpp
    src/SceneEffects.h src/SceneEffects.cpp
    src/UiControls.h src/UiControls.cpp
    src/UiIcons.cpp src/UiTheme.h src/UiTheme.cpp assets/icons.qrc
    src/AssetImport.h src/AssetImport.cpp
    src/MediaFrames.h src/MediaFrames.cpp
    src/FrameExport.h src/FrameExport.cpp
    src/Mesh.h src/Mesh.cpp src/MeshOps.cpp
    src/ElementDesign.h src/ElementDesign.cpp src/Animation.cpp src/DesignTexture.cpp src/Hierarchy.cpp
    src/BuildModel.h src/BuildModel.cpp
    src/BuildCanvas.h src/BuildCanvas.cpp
    src/BuildWorkspace.h src/BuildWorkspace.cpp
    src/AnimationEditor.h src/AnimationEditor.cpp
    src/AnimationCuePanel.h src/AnimationCuePanel.cpp
    src/CueSequence.h src/CueSequence.cpp
    src/PropertyInspector.h src/PropertyInspector.cpp
    src/LayerPanel.h src/LayerPanel.cpp)
target_include_directories(CuebaseBuildCore PUBLIC src)
target_link_libraries(CuebaseBuildCore PUBLIC Qt6::Widgets Qt6::Multimedia Qt6::Svg assimp::assimp PkgConfig::AV)

add_executable(CuebaseRender src/RenderMain.cpp)
target_link_libraries(CuebaseRender PRIVATE CuebaseBuildCore)

add_executable(Cuebase WIN32
    src/main.cpp
    src/MainWindow.h src/MainWindow.cpp
    src/AddMenu.h src/AddMenu.cpp
    src/HelpCenter.h src/HelpCenter.cpp src/AppLinks.h
    src/Playout.h src/Playout.cpp src/AnimationPlayout.cpp
    src/Continuity.h src/Continuity.cpp
    src/Replay.h src/Replay.cpp
    src/CrewWorkspace.h src/CrewWorkspace.cpp src/CrewPortal.h
    src/ViewportWidget.h src/ViewportWidget.cpp
    src/TimelineWidget.h src/TimelineWidget.cpp
    src/QueueStrip.h src/QueueStrip.cpp
    src/EditorPanel.h src/EditorPanel.cpp
    src/StudioElements.h src/StudioElements.cpp src/StudioCueBoard.h src/StudioCueBoard.cpp src/StudioDashboard.h src/StudioDashboard.cpp
    src/ControlPanel.h src/ControlPanel.cpp
    src/NetworkOutputPanel.h src/NetworkOutputPanel.cpp
    src/ColorManagement.h src/ColorManagement.cpp
    src/DragDrop.h src/DragDrop.cpp)
add_library(CuebaseStreaming STATIC src/NetworkOutput.h src/NetworkOutput.cpp)
target_include_directories(CuebaseStreaming PUBLIC src)
target_link_libraries(CuebaseStreaming PUBLIC Qt6::Gui Qt6::Multimedia Qt6::Network)
target_link_libraries(Cuebase PRIVATE CuebaseBuildCore CuebaseStreaming Qt6::Multimedia)
if(WIN32)
    target_link_libraries(Cuebase PRIVATE psapi)
endif()
target_compile_definitions(Cuebase PRIVATE "CUEBASE_VERSION=\"${PROJECT_VERSION}\"")

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(CuebaseBuildTests tests/BuildTests.cpp)
    target_link_libraries(CuebaseBuildTests PRIVATE CuebaseBuildCore Qt6::Test)
    add_test(NAME CuebaseBuild COMMAND CuebaseBuildTests -platform offscreen
        -o test-results.txt,txt -o test-results.xml,junitxml)
    add_executable(CuebaseStreamTests tests/StreamTests.cpp
        src/MainWindow.h src/MainWindow.cpp src/HelpCenter.h src/HelpCenter.cpp
        src/AddMenu.h src/AddMenu.cpp src/QueueStrip.h src/QueueStrip.cpp
        src/ControlPanel.h src/ControlPanel.cpp src/NetworkOutputPanel.h src/NetworkOutputPanel.cpp
        src/EditorPanel.h src/EditorPanel.cpp
    src/StudioElements.h src/StudioElements.cpp src/StudioCueBoard.h src/StudioCueBoard.cpp src/StudioDashboard.h src/StudioDashboard.cpp
        src/AnimationPlayout.cpp
        src/Continuity.h src/Continuity.cpp
    src/Replay.h src/Replay.cpp
        src/CrewWorkspace.h src/CrewWorkspace.cpp src/CrewPortal.h
        src/TimelineWidget.h src/TimelineWidget.cpp src/DragDrop.h src/DragDrop.cpp
        src/Playout.h src/Playout.cpp src/ViewportWidget.h src/ViewportWidget.cpp
        src/ColorManagement.h src/ColorManagement.cpp)
    target_link_libraries(CuebaseStreamTests PRIVATE CuebaseStreaming CuebaseBuildCore Qt6::Test Qt6::Widgets)
    if(WIN32)
        target_link_libraries(CuebaseStreamTests PRIVATE psapi)
    endif()
    add_test(NAME CuebaseStream COMMAND CuebaseStreamTests -platform offscreen
        -o stream-results.txt,txt -o stream-results.xml,junitxml)
    set_tests_properties(CuebaseStream PROPERTIES TIMEOUT 300)
endif()
