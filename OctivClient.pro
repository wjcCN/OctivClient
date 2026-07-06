QT += widgets network

CONFIG += c++17

TEMPLATE = app
TARGET = OctivClient

INCLUDEPATH += $$PWD
DEFINES += OCTIV_SOURCE_DIR=\\\"$$PWD\\\"

SOURCES += \
    main.cpp \
    MainWindow.cpp \
    network/OctivClient.cpp \
    parser/JsonParser.cpp \
    utils/Logger.cpp

HEADERS += \
    MainWindow.h \
    network/OctivClient.h \
    model/OctivData.h \
    parser/JsonParser.h \
    utils/Logger.h

FORMS += \
    MainWindow.ui
