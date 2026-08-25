project (screenshot)

cmake_minimum_required (VERSION 3.16)

find_package (Kadu REQUIRED CONFIG)

set (SOURCES
	configuration/gui/screenshot-configuration-ui-handler.cpp
	configuration/screen-shot-configuration.cpp
	gui/actions/screenshot-action.cpp
	gui/actions/screenshot-actions.cpp
	gui/graphics-items/handler-rect-item.cpp
	gui/graphics-items/selection-frame-item.cpp
	gui/widgets/crop-image-widget.cpp
	gui/widgets/screenshot-tool-box.cpp
	gui/widgets/screenshot-widget.cpp
	screen-shot-saver.cpp
	screenshot-module.cpp
	screenshot-notification-service.cpp
	screenshot-plugin-modules-factory.cpp
	screenshot-plugin-object.cpp
	screenshot-taker.cpp
	screenshot.cpp
)

set (CONFIGURATION_FILES
	data/configuration/screenshot.ui
)

if (UNIX AND NOT APPLE)
	# The screen belongs to the compositor; it is reached over D-Bus through the desktop portal.
	list (APPEND SOURCES portal-screenshot.cpp)
	set (PLUGIN_ADDITIONAL_QT_MODULES DBus)
endif ()

kadu_plugin (screenshot
	PLUGIN_SOURCES ${SOURCES}
	PLUGIN_CONFIGURATION_FILES ${CONFIGURATION_FILES}
	PLUGIN_LIBRARIES ${LIBRARIES}
	PLUGIN_ADDITIONAL_QT_MODULES ${PLUGIN_ADDITIONAL_QT_MODULES}
)
