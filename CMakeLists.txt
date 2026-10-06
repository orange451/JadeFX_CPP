cmake_minimum_required(VERSION 3.16)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
project(jadefx LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "iOS" OR EMSCRIPTEN)
    set(JADEFX_GLFM ON)
else()
    set(JADEFX_GLFM OFF)
endif()

set(JADEFX_SOURCES
    src/paint/Color.cpp
    src/time/LocalDate.cpp
    src/style/Stylesheet.cpp
    src/style/Theme.cpp
    src/scene/Node.cpp
    src/scene/Painter.cpp
    src/scene/layout/Region.cpp
    src/scene/layout/Pane.cpp
    src/scene/layout/StackPane.cpp
    src/scene/layout/DirectionalBox.cpp
    src/scene/layout/VBox.cpp
    src/scene/layout/GridPane.cpp
    src/scene/layout/FlowPane.cpp
    src/scene/layout/HBox.cpp
    src/scene/layout/BorderPane.cpp
    src/scene/controls/Labeled.cpp
    src/scene/controls/Label.cpp
    src/scene/controls/ButtonBase.cpp
    src/scene/controls/Button.cpp
    src/scene/controls/ToggleGroup.cpp
    src/scene/controls/ToggleButton.cpp
    src/scene/controls/RadioButton.cpp
    src/scene/controls/CheckBox.cpp
    src/scene/controls/ProgressBar.cpp
    src/scene/controls/Tooltip.cpp
    src/scene/controls/Alert.cpp
    src/scene/controls/TextField.cpp
    src/scene/controls/MenuItem.cpp
    src/scene/controls/Menu.cpp
    src/scene/controls/MenuButton.cpp
    src/scene/controls/MenuBar.cpp
    src/scene/controls/ColorChooser.cpp
    src/scene/controls/ColorPicker.cpp
    src/scene/controls/ComboBox.cpp
    src/scene/controls/ComboBoxBase.cpp
    src/scene/controls/DatePicker.cpp
    src/scene/controls/Notifications.cpp
    src/scene/controls/Tab.cpp
    src/scene/controls/TabPane.cpp
    src/scene/image/Image.cpp
    src/scene/image/ImageView.cpp
    src/scene/text/Font.cpp
    src/scene/text/GlyphRaster.cpp
    src/scene/text/StyledDocument.cpp
    src/scene/controls/StyledTextArea.cpp
    src/scene/controls/StyleClassedTextArea.cpp
    src/scene/controls/InlineCssTextArea.cpp
    src/scene/controls/CodeArea.cpp
    src/scene/controls/ScrollTrack.cpp
    src/scene/controls/ScrollBar.cpp
    src/scene/controls/ScrollPane.cpp
    src/scene/controls/ScrollSupport.cpp
    src/scene/controls/ControlChrome.cpp
    src/scene/controls/SelectionModel.cpp
    src/scene/controls/IndexedCell.cpp
    src/scene/controls/VirtualFlow.cpp
    src/scene/controls/RowViewBase.cpp
    src/scene/controls/ListView.cpp
    src/scene/controls/TableView.cpp
    src/scene/controls/Slider.cpp
    src/scene/controls/Spinner.cpp
    src/scene/controls/SplitPane.cpp
    src/scene/controls/TreeItem.cpp
    src/scene/controls/TreeView.cpp
    src/scene/DragAndDrop.cpp
    src/scene/Scene.cpp
    src/scene/SubScene.cpp
    src/stage/Stage.cpp
    src/application/Application.cpp
    src/application/RunLater.cpp
    src/gl/gl.cpp
    src/gl/Resources.cpp
    src/gl/UiRenderer.cpp
    src/platform/GlfmHost.cpp
)

if(APPLE)
    enable_language(OBJCXX)
    list(APPEND JADEFX_SOURCES src/platform/ErrorDialog_apple.mm src/platform/FolderDialog_apple.mm
         src/platform/CopyCursor_apple.mm)
else()
    list(APPEND JADEFX_SOURCES src/platform/ErrorDialog.cpp src/platform/FolderDialog.cpp)
endif()

if(NOT JADEFX_GLFM)
    list(APPEND JADEFX_SOURCES src/platform/GlfwHost.cpp src/platform/UtilityWindow.cpp src/platform/CopyCursor.cpp)
else()
    list(APPEND JADEFX_SOURCES src/platform/UtilityWindowStub.cpp)
endif()

add_library(jadefx STATIC ${JADEFX_SOURCES})
target_include_directories(jadefx
    PUBLIC include
    PRIVATE src src/gl
)
target_compile_definitions(jadefx PRIVATE "JADEFX_SOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}\"")
if(NOT EMSCRIPTEN AND NOT ANDROID)
    # The Linux folder dialog waits for zenity or kdialog on a thread.
    find_package(Threads REQUIRED)
    target_link_libraries(jadefx PUBLIC Threads::Threads)
endif()

# stb_truetype and stb_image are public-domain headers. Fetch them the same way
# as GLFW and GLFM, pinned to nothings/stb 2c980bb (the bytes this tree used to vendor).
include(FetchContent)
FetchContent_Declare(
    stb_truetype
    URL https://raw.githubusercontent.com/nothings/stb/2c980bb59875b0d32144a71867fbdebb2f77cd20/stb_truetype.h
    URL_HASH SHA256=ecd30b05e0dd4fea3a13c26810dd9e1992dc379049482c393d5a19e6b5090aab
    DOWNLOAD_NO_EXTRACT TRUE
    # CMake before 3.18 leaves an unextracted download outside SOURCE_DIR.
    DOWNLOAD_DIR "${FETCHCONTENT_BASE_DIR}/stb_truetype-src"
)
FetchContent_GetProperties(stb_truetype)
if(NOT stb_truetype_POPULATED)
    # The header has no CMakeLists, so populate it directly.
    if(POLICY CMP0169)
        cmake_policy(SET CMP0169 OLD)
    endif()
    FetchContent_Populate(stb_truetype)
endif()
FetchContent_Declare(
    stb_image
    URL https://raw.githubusercontent.com/nothings/stb/2c980bb59875b0d32144a71867fbdebb2f77cd20/stb_image.h
    URL_HASH SHA256=594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3
    DOWNLOAD_NO_EXTRACT TRUE
    # CMake before 3.18 leaves an unextracted download outside SOURCE_DIR.
    DOWNLOAD_DIR "${FETCHCONTENT_BASE_DIR}/stb_image-src"
)
FetchContent_GetProperties(stb_image)
if(NOT stb_image_POPULATED)
    if(POLICY CMP0169)
        cmake_policy(SET CMP0169 OLD)
    endif()
    FetchContent_Populate(stb_image)
endif()
target_include_directories(jadefx PRIVATE "${stb_truetype_SOURCE_DIR}" "${stb_image_SOURCE_DIR}")

# FreeType rasterizes glyphs on Linux and Android with light hinting, as Chrome
# does there. Windows uses DirectWrite; Apple platforms and the web keep stb_truetype.
if(WIN32 OR APPLE OR EMSCRIPTEN)
    set(JADEFX_FREETYPE_DEFAULT OFF)
else()
    set(JADEFX_FREETYPE_DEFAULT ON)
endif()
option(JADEFX_FREETYPE "Rasterize glyphs with FreeType" ${JADEFX_FREETYPE_DEFAULT})
if(JADEFX_FREETYPE)
    # Prefer the system FreeType (apt's libfreetype-dev). Otherwise download 2.13.3.
    if(NOT ANDROID)
        find_package(Freetype QUIET)
    endif()
    if(TARGET Freetype::Freetype)
        target_link_libraries(jadefx PRIVATE Freetype::Freetype)
    else()
        FetchContent_Declare(
            freetype
            URL https://download.savannah.gnu.org/releases/freetype/freetype-2.13.3.tar.xz
            URL_HASH SHA256=0550350666d427c74daeb85d5ac7bb353acba5f76956395995311a9c6f063289
        )
        # Outlines only. The optional compressed, bitmap, and shaping formats are not needed.
        set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
        set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
        set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
        set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
        set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
        set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(freetype)
        target_link_libraries(jadefx PRIVATE freetype)
    endif()
    target_compile_definitions(jadefx PRIVATE JADEFX_FREETYPE)
endif()

if(JADEFX_GLFM)
    target_compile_definitions(jadefx PUBLIC JADEFX_GLFM JADEFX_GLES)
    include(FetchContent)
    FetchContent_Declare(
        glfm
        GIT_REPOSITORY https://github.com/brackeen/glfm.git
        GIT_TAG fa7906ae8663d1d027691db9df265c8d42d0b2b0
    )
    FetchContent_GetProperties(glfm)
    if(NOT glfm_POPULATED)
        FetchContent_Populate(glfm)
    endif()
    target_include_directories(jadefx PRIVATE "${glfm_SOURCE_DIR}/include" "${glfm_SOURCE_DIR}/src")
    if(ANDROID)
        target_sources(jadefx PRIVATE "${glfm_SOURCE_DIR}/src/glfm_android.c")
        target_link_libraries(jadefx PUBLIC android log EGL GLESv3)
        set_property(TARGET jadefx PROPERTY POSITION_INDEPENDENT_CODE ON)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        enable_language(OBJC)
        target_sources(jadefx PRIVATE "${glfm_SOURCE_DIR}/src/glfm_apple.m")
        target_compile_definitions(jadefx PRIVATE GLFM_INCLUDE_METAL=0)
        target_link_libraries(jadefx PUBLIC
            "-framework UIKit"
            "-framework Foundation"
            "-framework CoreGraphics"
            "-framework QuartzCore"
            "-framework OpenGLES"
            "-framework CoreMotion"
            "-framework CoreHaptics"
        )
    elseif(EMSCRIPTEN)
        target_sources(jadefx PRIVATE "${glfm_SOURCE_DIR}/src/glfm_emscripten.c")
    endif()
else()
    # Prefer a system GLFW (Homebrew, vcpkg, apt's libglfw3-dev). Otherwise download
    # the pinned 3.5.1 source. Building that source on Linux needs the X11 and
    # Wayland development packages, because GLFW 3.5 enables both backends.
    find_package(glfw3 3.3 QUIET CONFIG)
    if(NOT TARGET glfw)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(GLFW3 QUIET IMPORTED_TARGET glfw3)
        endif()
    endif()
    if(TARGET glfw)
        set(JADEFX_GLFW_TARGET glfw)
    elseif(TARGET PkgConfig::GLFW3)
        set(JADEFX_GLFW_TARGET PkgConfig::GLFW3)
    else()
        include(FetchContent)
        FetchContent_Declare(
            glfw
            URL https://github.com/glfw/glfw/releases/download/3.5.1/glfw-3.5.1.zip
            URL_HASH SHA256=ea79bc5feffc254c87291980c2d0bce9acebb68c4983b79f961dcd2cb8a611a0
        )
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(glfw)
        set(JADEFX_GLFW_TARGET glfw)
    endif()
    target_link_libraries(jadefx PUBLIC ${JADEFX_GLFW_TARGET})
    if(WIN32)
        # The common item dialog behind showFolderDialog.
        target_link_libraries(jadefx PUBLIC ole32 shell32 uuid)
        # timeBeginPeriod, so the frame loop's wait wakes on time.
        target_link_libraries(jadefx PUBLIC winmm)
        # Glyph rasterization, hinted the way Edge draws text.
        if(NOT JADEFX_FREETYPE)
            target_link_libraries(jadefx PUBLIC dwrite)
        endif()
    endif()
    if(APPLE)
        target_link_libraries(jadefx PUBLIC "-framework Cocoa")
    endif()
endif()

if(MSVC)
    target_compile_options(jadefx PRIVATE /W4 /utf-8)
    set_source_files_properties(src/scene/text/Font.cpp PROPERTIES COMPILE_OPTIONS "/wd4505")
    set_source_files_properties(src/scene/image/Image.cpp PROPERTIES COMPILE_OPTIONS "/wd4505")
else()
    target_compile_options(jadefx PRIVATE -Wall -Wextra)
    set_source_files_properties(src/scene/text/Font.cpp PROPERTIES COMPILE_OPTIONS "-Wno-unused-function;-Wno-sign-compare")
    set_source_files_properties(src/scene/image/Image.cpp PROPERTIES COMPILE_OPTIONS "-Wno-unused-function;-Wno-sign-compare")
endif()

function(jadefx_attach_resources target)
    if(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
        # Shaders belong in Resources. Putting them beside the executable makes
        # codesign reject the bundle, because those files sit inside Contents/MacOS.
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/shaders"
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/fonts"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/res/shaders" "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/shaders"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/res/fonts" "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/fonts"
            COMMAND codesign --force --sign - "$<TARGET_BUNDLE_DIR:${target}>"
            VERBATIM
        )
    else()
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/shaders"
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/fonts"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/res/shaders" "$<TARGET_FILE_DIR:${target}>/shaders"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/res/fonts" "$<TARGET_FILE_DIR:${target}>/fonts"
            VERBATIM
        )
    endif()
endfunction()

# Warnings every desktop target builds with.
function(jadefx_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
endfunction()

# One desktop sample. On macOS it is an app bundle named title, identified as
# local.jadefx.<name>, or local.jadefx when name is empty.
function(jadefx_add_sample target name title)
    add_executable(${target} MACOSX_BUNDLE ${ARGN})
    target_link_libraries(${target} PRIVATE jadefx)
    jadefx_warnings(${target})
    jadefx_attach_resources(${target})
    if(APPLE)
        if(name STREQUAL "")
            set(identifier "local.jadefx")
        else()
            set(identifier "local.jadefx.${name}")
        endif()
        set_target_properties(${target} PROPERTIES
            OUTPUT_NAME "${title}"
            MACOSX_BUNDLE_BUNDLE_NAME "${title}"
            MACOSX_BUNDLE_GUI_IDENTIFIER "${identifier}"
            MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/macos/Info.plist.in"
        )
    endif()
endfunction()

if(NOT JADEFX_GLFM)
    jadefx_add_sample(jadefx-purple "" "JadeFX" examples/main.cpp examples/purple_app.cpp)
    jadefx_add_sample(jadefx-hello hello "JadeFX Hello" examples/hello.cpp)
    jadefx_add_sample(jadefx-border border "JadeFX Border" examples/border_pane.cpp)
    jadefx_add_sample(jadefx-tabs tabs "JadeFX Tabs" examples/tab_pane.cpp)
    jadefx_add_sample(jadefx-tree tree "JadeFX Tree" examples/tree_view.cpp)
    jadefx_add_sample(jadefx-lists lists "JadeFX Lists" examples/list_view.cpp)
    jadefx_add_sample(jadefx-split split "JadeFX Split" examples/split_pane.cpp)
    jadefx_add_sample(jadefx-rich rich "JadeFX Rich" examples/rich_text.cpp)
    jadefx_add_sample(jadefx-controls controls "JadeFX Controls" examples/controls.cpp)
    add_executable(jadefx-tests
        tests/layout_tests.cpp
        tests/rich_text_tests.cpp
        tests/button_tests.cpp
        tests/cursor_tests.cpp
        tests/pointer_lock_tests.cpp
        tests/mouse_button_tests.cpp
        tests/toggle_tests.cpp
        tests/radio_button_tests.cpp
        tests/check_box_tests.cpp
        tests/progress_bar_tests.cpp
        tests/tooltip_tests.cpp
        tests/alert_tests.cpp
        tests/text_field_tests.cpp
        tests/menu_tests.cpp
        tests/combo_box_tests.cpp
        tests/slider_tests.cpp
        tests/spinner_tests.cpp
        tests/split_pane_tests.cpp
        tests/image_tests.cpp
        tests/run_later_tests.cpp
        tests/tree_view_tests.cpp
        tests/scroll_pane_tests.cpp
        tests/list_view_tests.cpp
        tests/core_tests.cpp
        tests/table_view_tests.cpp
        tests/theme_tests.cpp
        tests/layout_pane_tests.cpp
        tests/color_picker_tests.cpp
        tests/date_picker_tests.cpp
        tests/drag_drop_tests.cpp
        tests/notification_tests.cpp
        tests/subscene_tests.cpp
        tests/uniform_cache_tests.cpp
        tests/glyph_atlas_tests.cpp
        tests/occluder_tests.cpp
        tests/frame_phase_tests.cpp
    )
    target_include_directories(jadefx-tests PRIVATE "${CMAKE_SOURCE_DIR}/src")
    target_link_libraries(jadefx-tests PRIVATE jadefx)
    jadefx_warnings(jadefx-tests)
elseif(ANDROID)
    add_library(jadefx-purple SHARED examples/purple_app.cpp examples/glfm_entry.cpp)
    target_link_libraries(jadefx-purple PRIVATE jadefx)
    target_compile_definitions(jadefx-purple PRIVATE JADEFX_GLFM)
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    add_executable(jadefx-purple MACOSX_BUNDLE examples/purple_app.cpp examples/glfm_entry.cpp)
    target_link_libraries(jadefx-purple PRIVATE jadefx)
    target_compile_definitions(jadefx-purple PRIVATE JADEFX_GLFM)
    set_target_properties(jadefx-purple PROPERTIES
        OUTPUT_NAME "JadeFX"
        MACOSX_BUNDLE_BUNDLE_NAME "JadeFX"
        MACOSX_BUNDLE_GUI_IDENTIFIER "local.jadefx"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/macos/Info.plist.in"
    )
    jadefx_attach_resources(jadefx-purple)
else()
    add_executable(jadefx-purple examples/purple_app.cpp examples/glfm_entry.cpp)
    target_link_libraries(jadefx-purple PRIVATE jadefx)
    target_compile_definitions(jadefx-purple PRIVATE JADEFX_GLFM)
    if(EMSCRIPTEN)
        target_link_options(jadefx-purple PRIVATE "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/res/shaders@shaders")
    endif()
endif()
