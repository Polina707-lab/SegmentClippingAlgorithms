cmake_minimum_required(VERSION 3.16)

project(SegmentClippingAlgorithms)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

add_executable(SegmentClippingAlgorithms
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    clippingcanvas.cpp
    clippingcanvas.h
    resources.qrc
)

target_link_libraries(SegmentClippingAlgorithms
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)
