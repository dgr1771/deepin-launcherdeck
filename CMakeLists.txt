cmake_minimum_required(VERSION 3.13)
project(deepin-launcherdeck VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Dtk6 REQUIRED COMPONENTS Core Gui Widget)
find_package(X11 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB REQUIRED IMPORTED_TARGET xcb)   # 全局热键的独立 xcb 连接

add_executable(deepin-launcherdeck
    src/main.cpp
    src/appscanner.cpp
    src/globalhotkey.cpp
    src/tarotpanel.cpp
)

target_include_directories(deepin-launcherdeck PRIVATE ${X11_INCLUDE_DIR})
target_link_libraries(deepin-launcherdeck PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets
    Dtk6::Core Dtk6::Gui Dtk6::Widget
    ${X11_LIBRARIES}
    PkgConfig::XCB
)

install(TARGETS deepin-launcherdeck RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/launcher-deck.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
