project (windows_integration)

cmake_minimum_required (VERSION 3.16)

find_package (Kadu REQUIRED CONFIG)

# QtWinExtras was removed in Qt6. KaWinExtras preserves the small API surface
# used here while its jump list, thumbnail toolbar, and taskbar integrations
# move to Win32 APIs.

set (SOURCES
	jump-list.cpp
	kawinextres/ka-win-jump-list.cpp
	kawinextres/ka-win-jump-list.h
	kawinextres/ka-win-taskbar-button.cpp
	kawinextres/ka-win-taskbar-button.h
	kawinextres/ka-win-thumbnail-toolbar.cpp
	kawinextres/ka-win-thumbnail-toolbar.h
	windows-integration-exports.h
	windows-integration-module.cpp
	windows-integration-plugin-modules-factory.cpp
	windows-integration-plugin-object.cpp
	windows-integration.cpp
	windows-jump-list-service.cpp
	windows-taskbar-progress.cpp
	windows-thumbnail-toolbar.cpp
)

if (WIN32)
	list (APPEND SOURCES
		windows-jump-list.cpp)
	set (LIBRARIES ole32 propsys shell32)
else (WIN32)
	list (APPEND SOURCES
		console-jump-list.cpp)
endif (WIN32)

kadu_plugin (windows_integration
	PLUGIN_SOURCES ${SOURCES}
	PLUGIN_LIBRARIES ${LIBRARIES}
)

kadu_add_plugin_test (windows_integration windows-jump-list-service)
