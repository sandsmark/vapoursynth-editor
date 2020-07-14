cmake_minimum_required(VERSION 3.9)
project(vsedit)

find_package(Qt5 COMPONENTS Widgets Network WebSockets)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(COMMON_SRC
    common-src/helpers.cpp
    common-src/settings/settings_definitions_core.cpp
    common-src/settings/settings_manager_core.cpp
    common-src/log/styled_log_view_core.cpp
    common-src/log/vs_editor_log_definitions.cpp
    common-src/vapoursynth/vs_script_library.cpp
    common-src/vapoursynth/vs_script_processor_structures.cpp
    common-src/vapoursynth/vapoursynth_script_processor.cpp
    common-src/frame_header_writers/frame_header_writer.cpp
    common-src/frame_header_writers/frame_header_writer_null.cpp
    common-src/frame_header_writers/frame_header_writer_y4m.cpp
    common-src/jobs/job.cpp
    common-src/jobs/job_variables.cpp
    common-src/application_instance_file_guard/application_instance_file_guard.cpp
)

set(COMMON_UI_SRC
    common-src/settings/settings_definitions.cpp
    common-src/settings/settings_manager.cpp
    common-src/timeline_slider/timeline_slider.cpp
    common-src/log/styled_log_view_structures.cpp
    common-src/log/log_styles_model.cpp
    common-src/log/styled_log_view_settings_dialog.cpp
    common-src/log/styled_log_view.cpp
    common-src/log/vs_editor_log.cpp
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

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_library(common OBJECT ${COMMON_SRC})
target_link_libraries(common Qt5::Core)

add_library(common-ui OBJECT ${COMMON_UI_SRC})
target_link_libraries(common-ui Qt5::Core Qt5::Widgets)

add_executable(vsedit ${VSEDIT_SRC}
    $<TARGET_OBJECTS:common>
    $<TARGET_OBJECTS:common-ui>
    )
target_include_directories(vsedit
    PRIVATE
    vsedit
    ${CMAKE_CURRENT_SOURCE_DIR}/vsedit/src
    )

add_executable(vsedit-job-server
    vsedit-job-server/src/jobs/jobs_manager.cpp
    vsedit-job-server/src/job_server.cpp
    vsedit-job-server/src/main.cpp

    $<TARGET_OBJECTS:common>
)

add_executable(vsedit-job-server-watcher
    vsedit-job-server-watcher/src/jobs/jobs_model.cpp
    vsedit-job-server-watcher/src/jobs/job_edit_dialog.cpp
    vsedit-job-server-watcher/src/jobs/job_dependencies_delegate.cpp
    vsedit-job-server-watcher/src/jobs/job_state_delegate.cpp
    vsedit-job-server-watcher/src/connect_to_server_dialog.cpp
    vsedit-job-server-watcher/src/trusted_clients_addresses_dialog.cpp
    vsedit-job-server-watcher/src/main_window.cpp
    vsedit-job-server-watcher/src/main.cpp

    vsedit-job-server-watcher/src/jobs/job_edit_dialog.ui
    vsedit-job-server-watcher/src/connect_to_server_dialog.ui
    vsedit-job-server-watcher/src/trusted_clients_addresses_dialog.ui
    vsedit-job-server-watcher/src/main_window.ui

    $<TARGET_OBJECTS:common>
    $<TARGET_OBJECTS:common-ui>
)
target_include_directories(vsedit-job-server-watcher
    PRIVATE
    vsedit-job-server-watcher/src
    )

target_link_libraries(vsedit
    Qt5::Core
    Qt5::Widgets
    Qt5::Network
)
target_link_libraries(vsedit-job-server
    Qt5::Core
    Qt5::WebSockets
)
target_link_libraries(vsedit-job-server-watcher
    Qt5::Core
    Qt5::Network
    Qt5::WebSockets
    Qt5::Widgets
)

set_target_properties(
    vsedit
    vsedit-job-server
    vsedit-job-server-watcher
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
