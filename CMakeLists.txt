cmake_minimum_required(VERSION 3.16)

project(OctivClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT Qt6_DIR)
    set(_OCTIV_QT_PREFIX_CANDIDATES)

    if(DEFINED ENV{QTDIR})
        list(APPEND _OCTIV_QT_PREFIX_CANDIDATES "$ENV{QTDIR}")
    endif()

    if(MSVC)
        list(APPEND _OCTIV_QT_PREFIX_CANDIDATES
            "D:/Programs/Qt/6.11.1/msvc2022_64"
        )
    elseif(MINGW)
        list(APPEND _OCTIV_QT_PREFIX_CANDIDATES
            "D:/Programs/Qt/6.11.1/mingw_64"
        )
    endif()

    list(APPEND _OCTIV_QT_PREFIX_CANDIDATES
        "D:/Programs/Qt/6.11.1/msvc2022_64"
        "D:/Programs/Qt/6.11.1/mingw_64"
    )

    foreach(_OCTIV_QT_PREFIX IN LISTS _OCTIV_QT_PREFIX_CANDIDATES)
        if(EXISTS "${_OCTIV_QT_PREFIX}/lib/cmake/Qt6/Qt6Config.cmake")
            list(PREPEND CMAKE_PREFIX_PATH "${_OCTIV_QT_PREFIX}")
            break()
        endif()
    endforeach()

    unset(_OCTIV_QT_PREFIX)
    unset(_OCTIV_QT_PREFIX_CANDIDATES)
endif()

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

target_compile_definitions(OctivClient PRIVATE
    OCTIV_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(OctivClient PRIVATE
    Qt6::Widgets
    Qt6::Network
)

if(WIN32 AND Qt6_DIR)
    get_filename_component(_OCTIV_QT_BIN_DIR "${Qt6_DIR}/../../../bin" ABSOLUTE)
    set_target_properties(OctivClient PROPERTIES
        VS_DEBUGGER_ENVIRONMENT "PATH=${_OCTIV_QT_BIN_DIR};%PATH%"
    )
    unset(_OCTIV_QT_BIN_DIR)
endif()
