cmake_minimum_required(VERSION 3.13)
project(deepin-launcherdeck VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)

# Qt6/DTK6（deepin 25）优先，回退 Qt5/DTK5（UOS 专业版 eagle）
find_package(Qt6 QUIET COMPONENTS Core Gui Widgets)
if (Qt6_FOUND)
    find_package(Dtk6 REQUIRED COMPONENTS Core Gui Widget)
    set(DECK_QT_LIBS Qt6::Core Qt6::Gui Qt6::Widgets)
    set(DECK_DTK_LIBS Dtk6::Core Dtk6::Gui Dtk6::Widget)
else()
    # UOS eagle 的 Qt 5.11：组件式 find_package 的目标生成有怪癖，逐模块最稳
    find_package(Qt5Core REQUIRED)
    find_package(Qt5Gui REQUIRED)
    find_package(Qt5Widgets REQUIRED)
    find_package(DtkCore REQUIRED)
    find_package(DtkGui REQUIRED)
    find_package(DtkWidget REQUIRED)
    set(DECK_QT_LIBS Qt5::Core Qt5::Gui Qt5::Widgets)
    set(DECK_DTK_LIBS Dtk::Core Dtk::Gui Dtk::Widget)
    add_compile_definitions(DECK_QT5)
endif()
find_package(X11 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB REQUIRED IMPORTED_TARGET xcb)   # 全局热键的独立 xcb 连接

add_executable(deepin-launcherdeck
    src/main.cpp
    src/appscanner.cpp
    src/deckcard.cpp
    src/freecell.cpp
    src/slotmachine.cpp
    src/globalhotkey.cpp
    src/tarotpanel.cpp
    src/upgrademanager.cpp
)

target_include_directories(deepin-launcherdeck PRIVATE ${X11_INCLUDE_DIR})
target_link_libraries(deepin-launcherdeck PRIVATE
${DECK_QT_LIBS}
    ${DECK_DTK_LIBS}
    ${X11_LIBRARIES}
    PkgConfig::XCB
)

install(TARGETS deepin-launcherdeck RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/deepin-launcherdeck.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES assets/deepin-launcherdeck.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps)
