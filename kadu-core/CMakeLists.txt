cmake_minimum_required (VERSION 3.16)

if (ENABLE_DEVELOPER_BUILD AND NOT MSVC AND NOT APPLE)
	set (FAST_LINKING_DEFAULT ON)
else ()
	set (FAST_LINKING_DEFAULT OFF)
endif ()
option (FAST_LINKING "Enable for linking optimized for low RAM (works best with GCC and GNU ld)" "${FAST_LINKING_DEFAULT}")

if (FAST_LINKING)
	message (STATUS "Fast (re)linking enabled")
else ()
	message (STATUS "Fast (re)linking disabled")
endif ()

include_directories ("${CMAKE_CURRENT_BINARY_DIR}")

set (kadu_SRCS
	activate.cpp
	attention-service.cpp
	languages-manager.cpp
	themes.cpp
	translation-loader.cpp
	updates.cpp

	os/single-application/long-lived-lock-file.cpp
	os/single-application/single-application.cpp

)

if (WIN32)
	list (APPEND kadu_SRCS
		kadu.rc
	)
endif ()

set (main_SRCS
	main.cpp

	os/single-application/long-lived-lock-file.cpp
	os/single-application/single-application.cpp
)

if (WIN32)
	list (APPEND main_SRCS
		kadu.rc
	)
endif ()

set (kadu_MOC_SRCS
	themes.h
	updates.h

)

set (kadu_SUBDIRS
	accounts
	actions
	avatars
	buddies
	chat
	chat-style
	compression
	configuration
	contacts
	core
	dom
	emoji
	execution-arguments
	file-system
	file-transfer
	formatted-string
	gui
	html
	icons
	identities
	menu
	message
	misc
	model
	multilogon
	network
	notification
	os
	parser
	plugin
	protocols
	qt
	roster
	services
	ssl
	status
	storage
	talkable
	task
	themes
	url-handlers
	widgets
	windows
)

find_package (LibArchive 2.6.0 REQUIRED)
# The Windows Conan package enables LZMA explicitly. check_library_exists()
# cannot probe the imported CMakeDeps target used by that package.
if (NOT WIN32)
	include (CheckLibraryExists)
	check_library_exists ("${LibArchive_LIBRARIES}" "archive_write_set_compression_lzma" "" have_libarchive_lzma_support)
	check_library_exists ("${LibArchive_LIBRARIES}" "archive_write_set_compression_xz" "" have_libarchive_xz_support)
	if (NOT have_libarchive_lzma_support OR NOT have_libarchive_xz_support)
		message (SEND_ERROR "Your libarchive does not have support for lzma and/or xz archives.")
	endif ()
endif ()

include_directories (${LibArchive_INCLUDE_DIRS})
list (APPEND ADDITIONAL_LIBKADU_LIBRARIES ${LibArchive_LIBRARIES})

# KWindowSystem carries the activation token a Wayland compositor wants before it lets a window
# raise itself, and reaches KWin's background blur. Both degrade quietly when it is absent.
find_package (KF6WindowSystem QUIET)
if (KF6WindowSystem_FOUND)
	set (HAVE_KWINDOWSYSTEM 1)
	message (STATUS "Found KF6WindowSystem: window activation and blur enabled")
	list (APPEND ADDITIONAL_LIBKADU_LIBRARIES KF6::WindowSystem)
else ()
	set (HAVE_KWINDOWSYSTEM 0)
	message (STATUS "KF6WindowSystem not found: windows will not raise themselves under Wayland")
endif ()

# KConfig is used for one thing only: telling the desktop's configuration to be read again when the
# desktop changes its colours. Breeze works its menu bar and tool bar colours out from the file
# named after the application, which does not notice kdeglobals changing underneath it, so without
# this those two keep the colours the desktop had when Kadu started.
find_package (KF6Config QUIET)
if (KF6Config_FOUND)
	set (HAVE_KCONFIG 1)
	message (STATUS "Found KF6Config: menu bar and toolbars follow the desktop's colours")
	list (APPEND ADDITIONAL_LIBKADU_LIBRARIES KF6::ConfigCore)
else ()
	set (HAVE_KCONFIG 0)
	message (STATUS "KF6Config not found: the menu bar and toolbars keep the colours Kadu started with")
endif ()

if (APPLE)
	list (APPEND ADDITIONAL_LIBKADU_LIBRARIES "-framework carbon")
endif ()

# Written here rather than at the top of the file: every HAVE_* answer above has to be known before
# the header that carries them is generated. Standing earlier, it wrote them all as zero, so the
# optional pieces were compiled out however many of their libraries were installed.
configure_file (kadu-config.h.in "${CMAKE_CURRENT_BINARY_DIR}/kadu-config.h" ESCAPE_QUOTES)

macro (kadu_subdirectory SUBDIR SRCS)
	if (FAST_LINKING)
		add_library (${SUBDIR}
			STATIC
			${SRCS}
		)
		kadu_set_flags (${SUBDIR})
		set_property (TARGET ${SUBDIR} PROPERTY POSITION_INDEPENDENT_CODE TRUE)
		target_compile_definitions (${SUBDIR} PRIVATE libkadu_EXPORTS)
	else ()
		set (tmp "")
		foreach (SRC ${SRCS})
			if (IS_ABSOLUTE "${SRC}")
				list (APPEND tmp "${SRC}")
			else ()
				list (APPEND tmp "${SUBDIR}/${SRC}")
			endif ()
		endforeach ()

		set (${SUBDIR}_SRCS "${tmp}" PARENT_SCOPE)
	endif ()
endmacro ()

foreach (ARG ${kadu_SUBDIRS})
	add_subdirectory (${ARG})

	if (NOT FAST_LINKING)
		list (APPEND kadu_SUBDIRS_SRCS ${${ARG}_SRCS} ${${ARG}_MOC_SRCS})
		include_directories (${${ARG}_INCLUDES})
	endif ()
endforeach ()

add_library (libkadu SHARED ${kadu_SRCS} ${kadu_MOC_SRCS} ${kadu_SUBDIRS_SRCS})
kadu_set_flags (libkadu)
set_property (TARGET libkadu PROPERTY PREFIX "")

find_package (Qt6 6.2 REQUIRED COMPONENTS QuickControls2)

qt_add_qml_module (libkadu
	URI Kadu.Chat
	VERSION 1.0
	NO_PLUGIN
	IMPORTS QtLocation QtPositioning QtQuick.Controls QtQuick.Effects
	QML_FILES
		chat/qml/EmojiPicker.qml
		chat/qml/ChatPage.qml
		chat/qml/TimelineEncryptionIndicator.qml
		chat/qml/TimelineActionsBar.qml
		chat/qml/TimelineReactionsBar.qml
		chat/qml/ReactionEmojiPickerPopup.qml

		chat/qml/TimelineMessagePopup.qml
		chat/qml/ChatFileAttachment.qml
		chat/qml/ChatImageAttachment.qml
		chat/qml/ImageViewerDialog.qml
		chat/qml/KaduImage.qml
		chat/qml/ChatLocation.qml
		chat/qml/LocationViewerDialog.qml
		chat/qml/styles/Bubbles/BubblesComposerContext.qml
		chat/qml/styles/Bubbles/BubblesComposerOverlay.qml
		chat/qml/styles/Bubbles/BubblesChatStyle.qml
		chat/qml/styles/Bubbles/BubblesPinnedMessages.qml
		chat/qml/styles/Bubbles/BubblesTimelineItem.qml
		chat/qml/styles/KaduClassic/KaduClassicChatStyle.qml
		chat/qml/styles/KaduClassic/KaduClassicComposerContext.qml
		chat/qml/styles/KaduClassic/KaduClassicComposerOverlay.qml
		chat/qml/styles/KaduClassic/KaduClassicPinnedMessages.qml
		chat/qml/styles/KaduClassic/KaduClassicTimelineItem.qml
		chat/qml/styles/Irssi/IrssiChatStyle.qml
		chat/qml/styles/Irssi/IrssiComposerOverlay.qml
		chat/qml/styles/Irssi/IrssiPinnedMessages.qml
		chat/qml/styles/Irssi/IrssiStatusBar.qml
		chat/qml/styles/Irssi/IrssiTimelineItem.qml
		widgets/qml/LocationSelector.qml
		widgets/qml/InfoPanel.qml
		widgets/qml/info-panel-styles/Classic/ClassicBuddyInfoStyle.qml
		widgets/qml/info-panel-styles/Compact/CompactBuddyInfoStyle.qml
)

if (FAST_LINKING)
	if (APPLE)
		target_link_libraries (libkadu LINK_PRIVATE -Wl,-all_load ${kadu_SUBDIRS})
	else ()
		target_link_libraries (libkadu LINK_PRIVATE -Wl,--start-group -Wl,--whole-archive ${kadu_SUBDIRS} -Wl,--no-whole-archive -Wl,--end-group)
	endif ()
endif ()

target_link_libraries (libkadu LINK_PRIVATE
		Qt6::Core Qt6::StateMachine Qt6::Gui Qt6::Network
		Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::QuickWidgets Qt6::Widgets Qt6::Xml
)
if (UNIX AND NOT APPLE)
	# X11Extras is gone in Qt6 -- absorbed into QtGui.
	target_link_libraries (libkadu LINK_PRIVATE Qt6::DBus)
endif ()
target_link_libraries (libkadu LINK_PRIVATE ${ADDITIONAL_LIBKADU_LIBRARIES})

add_executable (kadu WIN32 ${main_SRCS})
kadu_set_flags (kadu)
target_link_libraries (kadu LINK_PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Xml)
target_link_libraries (kadu LINK_PRIVATE libkadu ${ADDITIONAL_LIBRARIES})

if (NOT IS_ABSOLUTE "${KADU_INSTALL_LIB_DIR}")
	set (install_full_lib_dir "${CMAKE_INSTALL_PREFIX}/${KADU_INSTALL_LIB_DIR}")
else ()
	set (install_full_lib_dir "${KADU_INSTALL_LIB_DIR}")
endif ()

set_target_properties (kadu PROPERTIES
	INSTALL_RPATH "${install_full_lib_dir}/kadu"
	BUILD_WITH_INSTALL_RPATH TRUE
)

install (TARGETS libkadu kadu
	EXPORT KaduTargets
	LIBRARY DESTINATION ${KADU_INSTALL_LIB_DIR}/kadu
	RUNTIME DESTINATION ${KADU_INSTALL_BIN_DIR}
)
# TODO this WIN32 check is a workaround for an error on Linux
if (WIN32 AND KADU_INSTALL_SDK)
	install (TARGETS libkadu ARCHIVE DESTINATION ${KADU_INSTALL_SDK_DIR}/lib)
endif ()

if (WIN32)
	add_executable (kadu_c ${main_SRCS})
	kadu_set_flags (kadu_c)
	target_link_libraries (kadu_c LINK_PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Xml)
	target_link_libraries (kadu_c LINK_PRIVATE libkadu ${ADDITIONAL_LIBRARIES})

	target_compile_definitions (kadu_c PRIVATE KADU_CONSOLE)

	target_link_libraries (kadu LINK_PRIVATE ws2_32)
	target_link_libraries (libkadu LINK_PRIVATE ws2_32 ole32)
	target_link_libraries (kadu_c LINK_PRIVATE ws2_32)

	install (TARGETS kadu_c DESTINATION ${KADU_INSTALL_BIN_DIR})

	file (WRITE "${CMAKE_CURRENT_BINARY_DIR}/qt.conf" "[Paths]\nPrefix = .\nPlugins = qt/plugins\nQml2Imports = qt/qml\nTranslations = translations\n")
	install (FILES "${CMAKE_CURRENT_BINARY_DIR}/qt.conf" "kadu.ico" DESTINATION "${KADU_INSTALL_BIN_DIR}")
endif ()

install (TARGETS kadu DESTINATION ${KADU_INSTALL_BIN_DIR})

if (UNIX AND NOT APPLE)
	configure_file (kadu.desktop.in "${CMAKE_CURRENT_BINARY_DIR}/${KADU_DESKTOP_FILE_NAME}")
	install (FILES "${CMAKE_CURRENT_BINARY_DIR}/${KADU_DESKTOP_FILE_NAME}" DESTINATION "${KADU_DESKTOP_FILE_DIR}")

	install (FILES hi16-app-kadu.png DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/16x16/apps" RENAME kadu.png)
	install (FILES hi22-app-kadu.png DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/22x22/apps" RENAME kadu.png)
	install (FILES hi24-app-kadu.png DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/24x24/apps" RENAME kadu.png)
	install (FILES hi32-app-kadu.png DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/32x32/apps" RENAME kadu.png)
	install (FILES hi48-app-kadu.png DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/48x48/apps" RENAME kadu.png)
	install (FILES hi64-app-kadu.png DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/64x64/apps" RENAME kadu.png)
	install (FILES hi128-app-kadu.png DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/128x128/apps" RENAME kadu.png)
	install (FILES hi256-app-kadu.png DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps" RENAME kadu.png)
endif ()

# install api headers
# how to get list: find . -name '*.h' -exec dirname {} +|sort|uniq|sed -e 's|\./||' -e 's|^|\t|'
# TODO: consider using file (GLOB_RECURSE) command here
kadu_api_directories (kadu-core
	.
	accounts
	accounts/filter
	accounts/model
	actions
	actions/chat
	actions/chat-widget
	actions/main
	actions/search
	actions/talkable
	actions/tree-view
	avatars
	buddies
	buddies/model
	chat
	chat/model
	chat/timeline
	chat/type
	compression
	configuration
	contacts
	contacts/model
	core
	dom
	execution-arguments
	file-system
	file-transfer
	file-transfer/gui
	formatted-string
	gui
	gui/configuration
	gui/model
	gui/services
	html
	icons
	identities
	identities/model
	menu
	message
	misc
	model
	multilogon
	multilogon/model
	network
	network/proxy
	network/proxy/model
	notification
	notification/listener
	os
	os/generic
	os/qtsingleapplication
	parser
	plugin
	plugin/activation
	plugin/metadata
	plugin/model
	plugin/state
	protocols
	protocols/filter
	protocols/model
	protocols/services
	protocols/services/roster
	provider
	roster
	qt
	services
	ssl
	ssl/gui
	status
	storage
	talkable
	talkable/filter
	talkable/model
	task
	themes
	url-handlers
	widgets
	widgets/configuration
	widgets/dialog
	widgets/chat-widget
	widgets/group-tab-bar
	windows
	windows/open-chat-with
)
