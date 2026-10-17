# SPDX-FileCopyrightText: 2026 va111222666 / goldenbilly / Va126
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.16)

project(BetterWobblyWindows LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(QT_MAJOR_VERSION 6)

include(CTest)
option(BETTER_WOBBLY_BUILD_PLUGIN "Build the KWin effect and configuration module" ON)

# KDEInstallDirs must be loaded before Qt when the plugin is enabled. Qt's
# package files include GNUInstallDirs, and doing that first makes ECM inherit
# the GNU variables and print a warning for every install directory.
if(BETTER_WOBBLY_BUILD_PLUGIN)
    find_package(ECM REQUIRED NO_MODULE)
    list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})

    include(FeatureSummary)
    include(KDEInstallDirs)
    include(KDECompilerSettings NO_POLICY_SCOPE)
endif()

# The lightweight tests only need Qt. Keeping KWin/KF6 discovery inside the
# plugin block makes CI and source validation possible without a full Plasma SDK.
find_package(Qt6 REQUIRED COMPONENTS Core Gui)

set(BETTER_WOBBLYWINDOWS_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(BETTER_WOBBLY_BUILD_PLUGIN)
    find_package(Qt6 REQUIRED COMPONENTS Widgets DBus)
    find_package(KF6 REQUIRED COMPONENTS Config CoreAddons KCMUtils)
    find_package(KWin REQUIRED COMPONENTS kwineffects)

    set(BETTER_WOBBLYWINDOWS_COMMON_SOURCES
        src/Configuration/DefaultWobblyParameters.cpp
        src/EmbeddedBlur/EmbeddedBlurMath.cpp
        src/Plugin/TraceUtilities.cpp
    )

    set(BETTER_WOBBLYWINDOWS_EFFECT_SOURCES
        src/Plugin/PluginFactory.cpp
        src/Effect/WobblyWindowsEffect.cpp
        src/Effect/WobblyWindowMesh.cpp
        src/Effect/WindowLifecycle.cpp
        src/Physics/WobblyWindowPhysics.cpp
        src/Configuration/WobblyWindowsConfigStore.cpp
        src/Configuration/WobblyWindowsParameters.cpp
        src/EmbeddedBlur/EmbeddedBlurSetup.cpp
        src/EmbeddedBlur/EmbeddedBlurTargets.cpp
        src/EmbeddedBlur/EmbeddedBlurPaint.cpp
        src/EmbeddedBlur/ScopedEmbeddedBlurGlState.cpp
        resources/wobblyblur.qrc
        include/BetterWobblyWindows/WobblyWindowsEffect.h
    )

    add_library(better_wobblywindows MODULE
        ${BETTER_WOBBLYWINDOWS_COMMON_SOURCES}
        ${BETTER_WOBBLYWINDOWS_EFFECT_SOURCES}
    )

    # KWIN_EFFECT_FACTORY_SUPPORTED expands to a QObject factory. Teach CMake's
    # scanner about the KWin macro so the explicitly included PluginFactory.moc
    # is generated without the misleading "does not contain Q_OBJECT" warning.
    set_property(TARGET better_wobblywindows APPEND PROPERTY
        AUTOMOC_MACRO_NAMES KWIN_EFFECT_FACTORY_SUPPORTED
    )

    target_compile_definitions(better_wobblywindows PRIVATE
        KWIN_VERSION_MAJOR=${KWin_VERSION_MAJOR}
        KWIN_VERSION_MINOR=${KWin_VERSION_MINOR}
        KWIN_VERSION_PATCH=${KWin_VERSION_PATCH}
    )

    # Better Blur DX switches to the GLSL core shader set on
    # Plasma/KWin 6.7 because KWin changed shader loading there. Keep the same
    # boundary while retaining compatibility shaders for Plasma 6.5/6.6.
    if(KWin_VERSION VERSION_GREATER_EQUAL "6.6.90")
        target_compile_definitions(better_wobblywindows PRIVATE BETTER_WOBBLY_USE_CORE_SHADERS=1)
        message(STATUS "Better Wobbly Windows: using GLSL core blur shaders for KWin ${KWin_VERSION}")
    else()
        message(STATUS "Better Wobbly Windows: using GLSL compatibility blur shaders for KWin ${KWin_VERSION}")
    endif()

    target_include_directories(better_wobblywindows PRIVATE
        ${BETTER_WOBBLYWINDOWS_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Effect
    )

    if(TARGET KWin::kwin)
        set(KWIN_EFFECTS_TARGET KWin::kwin)
    elseif(TARGET KWin::kwineffects)
        set(KWIN_EFFECTS_TARGET KWin::kwineffects)
    else()
        message(FATAL_ERROR "Could not find a usable KWin effects CMake target")
    endif()

    target_link_libraries(better_wobblywindows PRIVATE
        ${KWIN_EFFECTS_TARGET}
        KF6::ConfigCore
        KF6::ConfigGui
        KF6::CoreAddons
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
    )

    install(TARGETS better_wobblywindows DESTINATION ${KDE_INSTALL_PLUGINDIR}/kwin/effects/plugins)

    add_library(better_wobblywindows_config MODULE
        src/Configuration/WobblyWindowsEffectConfig.cpp
        include/BetterWobblyWindows/WobblyWindowsEffectConfig.h
    )

    target_include_directories(better_wobblywindows_config PRIVATE ${BETTER_WOBBLYWINDOWS_INCLUDE_DIR})

    target_link_libraries(better_wobblywindows_config PRIVATE
        KF6::ConfigCore
        KF6::ConfigGui
        KF6::CoreAddons
        KF6::KCMUtils
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::DBus
    )

    install(TARGETS better_wobblywindows_config DESTINATION ${KDE_INSTALL_PLUGINDIR}/kwin/effects/configs)
endif()

if(BUILD_TESTING)
    function(AddBetterWobblyTest target)
        add_executable(${target} ${ARGN})
        target_include_directories(${target} PRIVATE ${BETTER_WOBBLYWINDOWS_INCLUDE_DIR})
        target_link_libraries(${target} PRIVATE Qt6::Core)
        add_test(NAME ${target} COMMAND ${target})
        set_tests_properties(${target} PROPERTIES TIMEOUT 10)
    endfunction()

    AddBetterWobblyTest(EmbeddedBlurMathTests
        tests/EmbeddedBlurMathTests.cpp
        src/EmbeddedBlur/EmbeddedBlurMath.cpp
    )
    target_link_libraries(EmbeddedBlurMathTests PRIVATE Qt6::Gui)

    AddBetterWobblyTest(EmbeddedBlurColorTransformTests
        tests/EmbeddedBlurColorTransformTests.cpp
        src/EmbeddedBlur/EmbeddedBlurMath.cpp
    )
    target_link_libraries(EmbeddedBlurColorTransformTests PRIVATE Qt6::Gui)

    AddBetterWobblyTest(EmbeddedBlurGeometryTests
        tests/EmbeddedBlurGeometryTests.cpp
        src/EmbeddedBlur/EmbeddedBlurMath.cpp
    )
    target_link_libraries(EmbeddedBlurGeometryTests PRIVATE Qt6::Gui)

    AddBetterWobblyTest(EmbeddedBlurTransitionTests
        tests/EmbeddedBlurTransitionTests.cpp
        src/EmbeddedBlur/EmbeddedBlurMath.cpp
    )
    target_link_libraries(EmbeddedBlurTransitionTests PRIVATE Qt6::Gui)

    AddBetterWobblyTest(DefaultWobblyParameterSetTests
        tests/DefaultWobblyParameterSetTests.cpp
        src/Configuration/DefaultWobblyParameters.cpp
    )

    AddBetterWobblyTest(TraceUtilitiesTests
        tests/TraceUtilitiesTests.cpp
        src/Plugin/TraceUtilities.cpp
    )

    AddBetterWobblyTest(TraceEnvironmentDisabledTests
        tests/TraceEnvironmentTests.cpp
        src/Plugin/TraceUtilities.cpp
    )
    target_compile_definitions(TraceEnvironmentDisabledTests PRIVATE EXPECTED_TRACE_ENABLED=0)

    AddBetterWobblyTest(TraceEnvironmentEnabledTests
        tests/TraceEnvironmentTests.cpp
        src/Plugin/TraceUtilities.cpp
    )
    target_compile_definitions(TraceEnvironmentEnabledTests PRIVATE EXPECTED_TRACE_ENABLED=1)

    AddBetterWobblyTest(BetterBlurDxApiTests
        tests/BetterBlurDxApiTests.cpp
    )

    AddBetterWobblyTest(WobblyWindowsConstantsTests
        tests/WobblyWindowsConstantsTests.cpp
    )
endif()

if(BETTER_WOBBLY_BUILD_PLUGIN)
    feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
endif()
