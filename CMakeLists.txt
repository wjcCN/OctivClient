cmake_minimum_required(VERSION 3.16)

project(OctivClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_DISABLE_FIND_PACKAGE_WrapVulkanHeaders ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

add_executable(OctivClient
    main.cpp
    MainWindow.h
    MainWindow.cpp
    MainWindow.ui
    network/OctivClient.h
    network/OctivClient.cpp
    model/OctivData.h
    parser/JsonParser.h
    parser/JsonParser.cpp
    utils/Logger.h
    utils/Logger.cpp
)

target_include_directories(OctivClient PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(OctivClient PRIVATE
    Qt6::Widgets
    Qt6::Network
)
