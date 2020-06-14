CONFIG += qt

QT += widgets
QT += network

QT_VERSION_WARNING = "WARNING: Linking against Qt version lower than 5.6.1 is likely to cause CLI tools video encoding to crash due to I/O but in Qt."

win32 {
	equals(QT_MAJOR_VERSION, 5) {
		equals(QT_MINOR_VERSION, 6):lessThan(QT_PATCH_VERSION, 1)) {
			message($$QT_VERSION_WARNING)
		}
		lessThan(QT_MINOR_VERSION, 6) {
			message($$QT_VERSION_WARNING)
		}
	}
}

HOST_64_BIT = contains(QMAKE_HOST.arch, "x86_64")
TARGET_64_BIT = contains(QMAKE_TARGET.arch, "x86_64")
ARCHITECTURE_64_BIT = $$HOST_64_BIT | $$TARGET_64_BIT

PROJECT_DIRECTORY = $$_PRO_FILE_PWD_/../../vsedit
COMMON_DIRECTORY = $$_PRO_FILE_PWD_/../..

CONFIG(debug, debug|release) {
	contains(QMAKE_COMPILER, msvc) {
		if($$ARCHITECTURE_64_BIT) {
			DESTDIR = $${COMMON_DIRECTORY}/build/debug-64bit-msvc
			TARGET = vsedit-debug-64bit-msvc
			OBJECTS_DIR = $${PROJECT_DIRECTORY}/generated/obj-debug-64bit-msvc
		} else {
			DESTDIR = $${COMMON_DIRECTORY}/build/debug-32bit-msvc
			TARGET = vsedit-debug-32bit-msvc
			OBJECTS_DIR = $${PROJECT_DIRECTORY}/generated/obj-debug-32bit-msvc
		}
	}

} else {
	contains(QMAKE_COMPILER, msvc) {
		if($$ARCHITECTURE_64_BIT) {
			DESTDIR = $${COMMON_DIRECTORY}/build/release-64bit-msvc
			TARGET = vsedit
			OBJECTS_DIR = $${PROJECT_DIRECTORY}/generated/obj-release-64bit-msvc
		} else {
			DESTDIR = $${COMMON_DIRECTORY}/build/release-32bit-msvc
			TARGET = vsedit-32bit
			OBJECTS_DIR = $${PROJECT_DIRECTORY}/generated/obj-release-32bit-msvc
		}
	}

	DEFINES += NDEBUG

}

S = $${DIR_SEPARATOR}

D = $$DESTDIR
D = $$replace(D, $$escape_expand(\\), $$S)
D = $$replace(D, /, $$S)

SC = $${COMMON_DIRECTORY}/
SC = $$replace(SC, $$escape_expand(\\), $$S)
SC = $$replace(SC, /, $$S)

E = $$escape_expand(\n\t)

macx {
	INCLUDEPATH += /usr/local/include
	ICON = $${COMMON_DIRECTORY}/resources/vsedit.icns
}

win32 {
	QT += winextras
QMAKE_POST_LINK += $${QMAKE_COPY} $${SC}$${S}resources$${S}vsedit.ico $${D}$${S}vsedit.ico $${E}
QMAKE_POST_LINK += $${QMAKE_COPY} $${SC}$${S}resources$${S}vsedit.svg $${D}$${S}vsedit.svg $${E}
QMAKE_POST_LINK += $${QMAKE_COPY} $${SC}$${S}README $${D}$${S}README $${E}
QMAKE_POST_LINK += $${QMAKE_COPY} $${SC}$${S}LICENSE $${D}$${S}LICENSE $${E}
QMAKE_POST_LINK += $${QMAKE_COPY} $${SC}$${S}CHANGELOG $${D}$${S}CHANGELOG $${E}


	INCLUDEPATH += 'C:/Program Files (x86)/VapourSynth/sdk/include/'

	DEPLOY_COMMAND = windeployqt
	DEPLOY_TARGET = $$shell_quote($$shell_path($${D}/$${TARGET}.exe))
	QMAKE_POST_LINK += $${DEPLOY_COMMAND} --no-translations $${DEPLOY_TARGET} $${E}

	if($$ARCHITECTURE_64_BIT) {
		message("x86_64 build")
	} else {
		message("x86 build")
		contains(QMAKE_COMPILER, gcc) {
			QMAKE_LFLAGS += -Wl,--large-address-aware
		}
		contains(QMAKE_COMPILER, msvc) {
			QMAKE_LFLAGS += /LARGEADDRESSAWARE
		}
	}
}

include($${COMMON_DIRECTORY}/pro/common.pri)

TEMPLATE = app

RC_ICONS = $${COMMON_DIRECTORY}/resources/vsedit.ico
QMAKE_TARGET_PRODUCT = 'VapourSynth Editor'
QMAKE_TARGET_DESCRIPTION = 'VapourSynth script editor'

#SUBDIRS

MOC_DIR = $${PROJECT_DIRECTORY}/generated/moc
UI_DIR = $${PROJECT_DIRECTORY}/generated/ui
RCC_DIR = $${PROJECT_DIRECTORY}/generated/rcc

#DEFINES

INCLUDEPATH += $${PROJECT_DIRECTORY}
INCLUDEPATH += $${COMMON_DIRECTORY}

#TRANSLATIONS

RESOURCES = $${COMMON_DIRECTORY}/resources/vsedit.qrc

FORMS += $${COMMON_DIRECTORY}/common-src/log/styled_log_view_settings_dialog.ui

FORMS += $${PROJECT_DIRECTORY}/src/settings/settings_dialog.ui
FORMS += $${PROJECT_DIRECTORY}/src/script_status_bar_widget/script_status_bar_widget.ui
FORMS += $${PROJECT_DIRECTORY}/src/preview/preview_advanced_settings_dialog.ui
FORMS += $${PROJECT_DIRECTORY}/src/preview/preview_dialog.ui
FORMS += $${PROJECT_DIRECTORY}/src/frame_consumers/benchmark_dialog.ui
FORMS += $${PROJECT_DIRECTORY}/src/frame_consumers/encode_dialog.ui
FORMS += $${PROJECT_DIRECTORY}/src/script_templates/templates_dialog.ui
FORMS += $${PROJECT_DIRECTORY}/src/main_window.ui

HEADERS += $${COMMON_DIRECTORY}/common-src/helpers.h
HEADERS += $${COMMON_DIRECTORY}/common-src/aligned_vector.h
HEADERS += $${COMMON_DIRECTORY}/common-src/chrono.h
HEADERS += $${COMMON_DIRECTORY}/common-src/settings/settings_definitions_core.h
HEADERS += $${COMMON_DIRECTORY}/common-src/settings/settings_definitions.h
HEADERS += $${COMMON_DIRECTORY}/common-src/settings/settings_manager_core.h
HEADERS += $${COMMON_DIRECTORY}/common-src/settings/settings_manager.h
HEADERS += $${COMMON_DIRECTORY}/common-src/log/styled_log_view_core.h
HEADERS += $${COMMON_DIRECTORY}/common-src/log/styled_log_view_structures.h
HEADERS += $${COMMON_DIRECTORY}/common-src/log/log_styles_model.h
HEADERS += $${COMMON_DIRECTORY}/common-src/log/styled_log_view_settings_dialog.h
HEADERS += $${COMMON_DIRECTORY}/common-src/log/styled_log_view.h
HEADERS += $${COMMON_DIRECTORY}/common-src/log/vs_editor_log_definitions.h
HEADERS += $${COMMON_DIRECTORY}/common-src/log/vs_editor_log.h
HEADERS += $${COMMON_DIRECTORY}/common-src/vapoursynth/vs_script_library.h
HEADERS += $${COMMON_DIRECTORY}/common-src/vapoursynth/vs_script_processor_structures.h
HEADERS += $${COMMON_DIRECTORY}/common-src/vapoursynth/vapoursynth_script_processor.h
HEADERS += $${COMMON_DIRECTORY}/common-src/frame_header_writers/frame_header_writer.h
HEADERS += $${COMMON_DIRECTORY}/common-src/frame_header_writers/frame_header_writer_null.h
HEADERS += $${COMMON_DIRECTORY}/common-src/frame_header_writers/frame_header_writer_y4m.h
HEADERS += $${COMMON_DIRECTORY}/common-src/jobs/job.h
HEADERS += $${COMMON_DIRECTORY}/common-src/jobs/job_variables.h
HEADERS += $${COMMON_DIRECTORY}/common-src/timeline_slider/timeline_slider.h

HEADERS += $${PROJECT_DIRECTORY}/src/settings/actions_hotkey_edit_model.h
HEADERS += $${PROJECT_DIRECTORY}/src/settings/clearable_key_sequence_editor.h
HEADERS += $${PROJECT_DIRECTORY}/src/settings/item_delegate_for_hotkey.h
HEADERS += $${PROJECT_DIRECTORY}/src/settings/theme_elements_model.h
HEADERS += $${PROJECT_DIRECTORY}/src/settings/settings_dialog.h
HEADERS += $${PROJECT_DIRECTORY}/src/script_status_bar_widget/script_status_bar_widget.h
HEADERS += $${PROJECT_DIRECTORY}/src/preview/scroll_navigator.h
HEADERS += $${PROJECT_DIRECTORY}/src/preview/preview_area.h
HEADERS += $${PROJECT_DIRECTORY}/src/preview/preview_advanced_settings_dialog.h
HEADERS += $${PROJECT_DIRECTORY}/src/preview/preview_dialog.h
HEADERS += $${PROJECT_DIRECTORY}/src/script_editor/number_matcher.h
HEADERS += $${PROJECT_DIRECTORY}/src/script_editor/syntax_highlighter.h
HEADERS += $${PROJECT_DIRECTORY}/src/script_editor/script_completer_model.h
HEADERS += $${PROJECT_DIRECTORY}/src/script_editor/script_completer.h
HEADERS += $${PROJECT_DIRECTORY}/src/script_editor/script_editor.h
HEADERS += $${PROJECT_DIRECTORY}/src/script_editor/fakevim/fakevimhandler.h
HEADERS += $${PROJECT_DIRECTORY}/src/script_editor/fakevim/fakevimactions.h
HEADERS += $${PROJECT_DIRECTORY}/src/script_editor/fakevim/fakevimtr.h
HEADERS += $${PROJECT_DIRECTORY}/src/script_editor/fakevim/utils/qtcassert.h
HEADERS += $${PROJECT_DIRECTORY}/src/vapoursynth/vs_plugin_data.h
HEADERS += $${PROJECT_DIRECTORY}/src/vapoursynth/vapoursynth_plugins_manager.h
HEADERS += $${PROJECT_DIRECTORY}/src/vapoursynth/vs_script_processor_dialog.h
HEADERS += $${PROJECT_DIRECTORY}/src/job_server_watcher_socket.h
HEADERS += $${PROJECT_DIRECTORY}/src/frame_consumers/benchmark_dialog.h
HEADERS += $${PROJECT_DIRECTORY}/src/frame_consumers/encode_dialog.h
HEADERS += $${PROJECT_DIRECTORY}/src/script_templates/drop_file_category_model.h
HEADERS += $${PROJECT_DIRECTORY}/src/script_templates/templates_dialog.h
HEADERS += $${PROJECT_DIRECTORY}/src/main_window.h

SOURCES += $${COMMON_DIRECTORY}/common-src/helpers.cpp
SOURCES += $${COMMON_DIRECTORY}/common-src/settings/settings_definitions_core.cpp
SOURCES += $${COMMON_DIRECTORY}/common-src/settings/settings_definitions.cpp
SOURCES += $${COMMON_DIRECTORY}/common-src/settings/settings_manager_core.cpp
SOURCES += $${COMMON_DIRECTORY}/common-src/settings/settings_manager.cpp
SOURCES += $${COMMON_DIRECTORY}/common-src/log/styled_log_view_core.cpp
SOURCES += $${COMMON_DIRECTORY}/common-src/log/styled_log_view_structures.cpp
SOURCES += $${COMMON_DIRECTORY}/common-src/log/log_styles_model.cpp
SOURCES += $${COMMON_DIRECTORY}/common-src/log/styled_log_view_settings_dialog.cpp
SOURCES += $${COMMON_DIRECTORY}/common-src/log/styled_log_view.cpp
SOURCES += $${COMMON_DIRECTORY}/common-src/log/vs_editor_log_definitions.cpp
SOURCES += $${COMMON_DIRECTORY}/common-src/log/vs_editor_log.cpp
SOURCES += $${COMMON_DIRECTORY}/common-src/vapoursynth/vs_script_library.cpp
SOURCES += $${COMMON_DIRECTORY}/common-src/vapoursynth/vs_script_processor_structures.cpp
SOURCES += $${COMMON_DIRECTORY}/common-src/vapoursynth/vapoursynth_script_processor.cpp
SOURCES += $${COMMON_DIRECTORY}/common-src/frame_header_writers/frame_header_writer.cpp
SOURCES += $${COMMON_DIRECTORY}/common-src/frame_header_writers/frame_header_writer_null.cpp
SOURCES += $${COMMON_DIRECTORY}/common-src/frame_header_writers/frame_header_writer_y4m.cpp
SOURCES += $${COMMON_DIRECTORY}/common-src/jobs/job.cpp
SOURCES += $${COMMON_DIRECTORY}/common-src/jobs/job_variables.cpp
SOURCES += $${COMMON_DIRECTORY}/common-src/timeline_slider/timeline_slider.cpp

SOURCES += $${PROJECT_DIRECTORY}/src/settings/actions_hotkey_edit_model.cpp
SOURCES += $${PROJECT_DIRECTORY}/src/settings/clearable_key_sequence_editor.cpp
SOURCES += $${PROJECT_DIRECTORY}/src/settings/item_delegate_for_hotkey.cpp
SOURCES += $${PROJECT_DIRECTORY}/src/settings/theme_elements_model.cpp
SOURCES += $${PROJECT_DIRECTORY}/src/settings/settings_dialog.cpp
SOURCES += $${PROJECT_DIRECTORY}/src/script_status_bar_widget/script_status_bar_widget.cpp
SOURCES += $${PROJECT_DIRECTORY}/src/preview/scroll_navigator.cpp
SOURCES += $${PROJECT_DIRECTORY}/src/preview/preview_area.cpp
SOURCES += $${PROJECT_DIRECTORY}/src/preview/preview_advanced_settings_dialog.cpp
SOURCES += $${PROJECT_DIRECTORY}/src/preview/preview_dialog.cpp
SOURCES += $${PROJECT_DIRECTORY}/src/script_editor/number_matcher.cpp
SOURCES += $${PROJECT_DIRECTORY}/src/script_editor/syntax_highlighter.cpp
SOURCES += $${PROJECT_DIRECTORY}/src/script_editor/script_completer_model.cpp
SOURCES += $${PROJECT_DIRECTORY}/src/script_editor/script_completer.cpp
SOURCES += $${PROJECT_DIRECTORY}/src/script_editor/script_editor.cpp
SOURCES += $${PROJECT_DIRECTORY}/src/script_editor/fakevim/fakevimhandler.cpp
SOURCES += $${PROJECT_DIRECTORY}/src/script_editor/fakevim/fakevimactions.cpp
SOURCES += $${PROJECT_DIRECTORY}/src/script_editor/fakevim/utils/qtcassert.cpp
SOURCES += $${PROJECT_DIRECTORY}/src/vapoursynth/vs_plugin_data.cpp
SOURCES += $${PROJECT_DIRECTORY}/src/vapoursynth/vapoursynth_plugins_manager.cpp
SOURCES += $${PROJECT_DIRECTORY}/src/vapoursynth/vs_script_processor_dialog.cpp
SOURCES += $${PROJECT_DIRECTORY}/src/job_server_watcher_socket.cpp
SOURCES += $${PROJECT_DIRECTORY}/src/frame_consumers/benchmark_dialog.cpp
SOURCES += $${PROJECT_DIRECTORY}/src/frame_consumers/encode_dialog.cpp
SOURCES += $${PROJECT_DIRECTORY}/src/script_templates/drop_file_category_model.cpp
SOURCES += $${PROJECT_DIRECTORY}/src/script_templates/templates_dialog.cpp
SOURCES += $${PROJECT_DIRECTORY}/src/main_window.cpp
SOURCES += $${PROJECT_DIRECTORY}/src/main.cpp

include($${COMMON_DIRECTORY}/pro/local_quirks.pri)
