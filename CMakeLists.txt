cmake_minimum_required(VERSION 3.13)
project(deepin-launcherdeck VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)   # UOS Qt5 -reduce-relocations 要求 -fPIC
add_compile_options(-fPIC)   # 双保险：POSITION_INDEPENDENT_CODE 在部分老 cmake 组合下不传导
include(GNUInstallDirs)

# Qt6/DTK6（deepin 25）优先；回退：UOS 专业版 eagle 的裁剪 Qt5.11/DTK5 无 cmake
# 导入目标 -> 全走 pkg-config 变量链接（.pc 文件两侧都有，绕开 config 怪癖）
find_package(Qt6 QUIET COMPONENTS Core Gui Widgets)
if (Qt6_FOUND)
    find_package(Dtk6 REQUIRED COMPONENTS Core Gui Widget)
    set(DECK_QT_LIBS Qt6::Core Qt6::Gui Qt6::Widgets)
    set(DECK_DTK_LIBS Dtk6::Core Dtk6::Gui Dtk6::Widget)
else()
    find_package(Qt5Core QUIET)   # 只为拿 moc 路径供 AUTOMOC 用（库仍走 pkg-config）
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(DECK_QT5CORE REQUIRED Qt5Core)
    pkg_check_modules(DECK_QT5GUI REQUIRED Qt5Gui)
    pkg_check_modules(DECK_QT5WID REQUIRED Qt5Widgets)
    pkg_check_modules(DECK_DTKCORE REQUIRED dtkcore)
    pkg_check_modules(DECK_DTKGUI REQUIRED dtkgui)
    pkg_check_modules(DECK_DTKWIDGET REQUIRED dtkwidget)
    set(DECK_QT_LIBS ${DECK_QT5CORE_LIBRARIES} ${DECK_QT5GUI_LIBRARIES} ${DECK_QT5WID_LIBRARIES}
                     ${DECK_DTKCORE_LIBRARIES} ${DECK_DTKGUI_LIBRARIES} ${DECK_DTKWIDGET_LIBRARIES})
    include_directories(${DECK_QT5CORE_INCLUDE_DIRS} ${DECK_QT5GUI_INCLUDE_DIRS} ${DECK_QT5WID_INCLUDE_DIRS}
                        ${DECK_DTKCORE_INCLUDE_DIRS} ${DECK_DTKGUI_INCLUDE_DIRS} ${DECK_DTKWIDGET_INCLUDE_DIRS})
    add_compile_options(${DECK_QT5CORE_CFLAGS_OTHER} ${DECK_QT5GUI_CFLAGS_OTHER} ${DECK_QT5WID_CFLAGS_OTHER}
                        ${DECK_DTKCORE_CFLAGS_OTHER} ${DECK_DTKGUI_CFLAGS_OTHER} ${DECK_DTKWIDGET_CFLAGS_OTHER})
    foreach(d ${Qt5Core_DEFINITIONS} ${Qt5Gui_DEFINITIONS} ${Qt5Widgets_DEFINITIONS})
        string(REGEX REPLACE "^-D" "" d2 "${d}")
        add_compile_definitions(${d2})
    endforeach()
    add_compile_definitions(DECK_QT5)
endif()
set(CMAKE_AUTOMOC ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)   # UOS Qt5 -reduce-relocations 要求 -fPIC
add_compile_options(-fPIC)   # 双保险

find_package(X11 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB REQUIRED IMPORTED_TARGET xcb)   # 全局热键的独立 xcb 连接

message("FACTS: Qt6_FOUND=${Qt6_FOUND} Qt5Core_DIR=${Qt5Core_DIR}")
message("FACTS: DECK_QT_LIBS=[${DECK_QT_LIBS}]")
if (NOT TARGET Qt5::Core)
    message("FACTS: Qt5::Core TARGET MISSING AT CONFIGURE")
else()
    message("FACTS: Qt5::Core TARGET EXISTS AT CONFIGURE")
endif()
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

target_link_libraries(deepin-launcherdeck PRIVATE
${DECK_QT_LIBS}
    ${DECK_DTK_LIBS}
    ${X11_LIBRARIES}
    PkgConfig::XCB
)

install(TARGETS deepin-launcherdeck RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/deepin-launcherdeck.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES assets/deepin-launcherdeck.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps)

get_target_property(_am deepin-launcherdeck AUTOMOC)
message("AUTOMOC_CHECK: target AUTOMOC = ${_am}")
get_directory_property(_dirvars COMPILE_DEFINITIONS)
message("DIR_DEFS: ${_dirvars}")
