cmake_minimum_required(VERSION 3.9)
project(vsedit)

find_package(Qt5 COMPONENTS Widgets Network)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(COMMON_SRC
    common-src/helpers.cpp
    common-src/settings/settings_definitions_core.cpp
    common-src/settings/settings_definitions.cpp
    common-src/settings/settings_manager_core.cpp
    common-src/settings/settings_manager.cpp
    common-src/log/styled_log_view_core.cpp
    common-src/log/styled_log_view_structures.cpp
    common-src/log/log_styles_model.cpp
    common-src/log/styled_log_view_settings_dialog.cpp
    common-src/log/styled_log_view.cpp
    common-src/log/vs_editor_log_definitions.cpp
    common-src/log/vs_editor_log.cpp
    common-src/vapoursynth/vs_script_library.cpp
    common-src/vapoursynth/vs_script_processor_structures.cpp
    common-src/vapoursynth/vapoursynth_script_processor.cpp
    common-src/frame_header_writers/frame_header_writer.cpp
    common-src/frame_header_writers/frame_header_writer_null.cpp
    common-src/frame_header_writers/frame_header_writer_y4m.cpp
    common-src/jobs/job.cpp
    common-src/jobs/job_variables.cpp
    common-src/timeline_slider/timeline_slider.cpp
)

set(VSEDIT_SRC
    vsedit/src/settings/actions_hotkey_edit_model.cpp
    vsedit/src/settings/clearable_key_sequence_editor.cpp
    vsedit/src/settings/item_delegate_for_hotkey.cpp
    vsedit/src/settings/theme_elements_model.cpp
    vsedit/src/settings/settings_dialog.cpp
    vsedit/src/script_status_bar_widget/script_status_bar_widget.cpp
    vsedit/src/preview/scroll_navigator.cpp
    vsedit/src/preview/preview_area.cpp
    vsedit/src/preview/preview_advanced_settings_dialog.cpp
    vsedit/src/preview/preview_dialog.cpp
    vsedit/src/script_editor/number_matcher.cpp
    vsedit/src/script_editor/syntax_highlighter.cpp
    vsedit/src/script_editor/script_completer_model.cpp
    vsedit/src/script_editor/script_completer.cpp
    vsedit/src/script_editor/script_editor.cpp
    vsedit/src/script_editor/fakevim/editor.cpp
    vsedit/src/script_editor/fakevim/fakevimhandler.cpp
    vsedit/src/script_editor/fakevim/fakevimactions.cpp
    vsedit/src/script_editor/fakevim/utils/qtcassert.cpp
    vsedit/src/vapoursynth/vs_plugin_data.cpp
    vsedit/src/vapoursynth/vapoursynth_plugins_manager.cpp
    vsedit/src/vapoursynth/vs_script_processor_dialog.cpp
    vsedit/src/job_server_watcher_socket.cpp
    vsedit/src/frame_consumers/benchmark_dialog.cpp
    vsedit/src/frame_consumers/encode_dialog.cpp
    vsedit/src/script_templates/drop_file_category_model.cpp
    vsedit/src/script_templates/templates_dialog.cpp
    vsedit/src/main_window.cpp
    vsedit/src/main.cpp

    resources/vsedit.qrc
    qdarkstyle/style.qrc
)

add_library(common OBJECT ${COMMON_SRC})
target_link_libraries(common
    Qt5::Core
    Qt5::Widgets
    )

add_executable(vsedit ${VSEDIT_SRC} $<TARGET_OBJECTS:common>)
include_directories(vsedit ${CMAKE_CURRENT_SOURCE_DIR}/vsedit/src ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(vsedit
    Qt5::Core
    Qt5::Widgets
    Qt5::Network
)
