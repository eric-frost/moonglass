cmake_minimum_required(VERSION 3.20)
project(moonglass)
set(CMAKE_CXX_STANDARD 20)
set(KF6_MIN_VERSION "6.0.0")
find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS Config ConfigWidgets CoreAddons GlobalAccel I18n Package KCMUtils)
find_package(KWin REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Gui DBus Widgets)

# Effect plugin
set(moonglass_SRCS
    moonglass.cpp
    moonglass.qrc
)

kcoreaddons_add_plugin(kwin-effect-moonglass SOURCES ${moonglass_SRCS} INSTALL_NAMESPACE kwin/effects/plugins)
target_link_libraries(kwin-effect-moonglass PRIVATE
    KWin::kwin
    KF6::GlobalAccel
    KF6::I18n
    KF6::ConfigCore
    KF6::ConfigGui
    Qt6::Core
    Qt6::Gui
)

# KCM config plugin
set(kwin_moonglass_config_SRCS moonglass_config.cpp)
ki18n_wrap_ui(kwin_moonglass_config_SRCS package/contents/ui/config.ui)
kconfig_add_kcfg_files(kwin_moonglass_config_SRCS moonglassconfig.kcfgc)

kcoreaddons_add_plugin(kwin_moonglass_config SOURCES ${kwin_moonglass_config_SRCS} INSTALL_NAMESPACE kwin/effects/configs)
target_link_libraries(kwin_moonglass_config PRIVATE
    KF6::KCMUtils
    KF6::CoreAddons
    KF6::I18n
    KF6::ConfigCore
    KF6::ConfigGui
    KF6::ConfigWidgets
    Qt6::DBus
    Qt6::Widgets
)

# Install package
kpackage_install_package(package kwin-effect-moonglass kwin/effects)
