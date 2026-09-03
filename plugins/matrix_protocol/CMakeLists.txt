project (matrix_protocol)

cmake_minimum_required (VERSION 3.16)

find_package (Kadu REQUIRED CONFIG)
find_package (QuotientQt6 REQUIRED)

set (SOURCES
    gui/matrix-power-level-editor.cpp
    gui/matrix-device-verification-dialog.cpp
    gui/matrix-restore-recovery-key-dialog.cpp
    gui/matrix-room-invitation-dialog.cpp
    gui/matrix-room-settings-window.cpp
    gui/matrix-add-account-widget.cpp
    gui/matrix-edit-account-widget.cpp
    matrix-account-data.cpp
    matrix-account-avatar-service.cpp
    matrix-chat-service.cpp
    matrix-chat-state-service.cpp
    matrix-contact-avatar-service.cpp
    matrix-device-verification-notification-service.cpp
    matrix-history-service.cpp
    matrix-megolm-session-recovery.cpp
    matrix-timeline-service.cpp
    matrix-id-validator.cpp
    matrix-join-room-action.cpp
    matrix-module.cpp
    matrix-plugin-modules-factory.cpp
    matrix-plugin-object.cpp
    matrix-protocol-factory.cpp
    matrix-protocol.cpp
    matrix-room-invitation-notification-service.cpp
    matrix-room-members-model.cpp
    matrix-verify-device-action.cpp
)

kadu_plugin (matrix_protocol
	PLUGIN_SOURCES ${SOURCES}
	PLUGIN_LIBRARIES QuotientQt6
	PLUGIN_ADDITIONAL_QT_MODULES Concurrent
)

# LibQuotient public headers use C++23 facilities (for example std::to_underlying).
# Keep the wider Kadu build at C++20; only this consumer needs the newer standard.
target_compile_features (matrix_protocol PRIVATE cxx_std_23)

# Quotient uses __VA_OPT__ in public event macros.  MSVC needs its
# conforming preprocessor explicitly enabled for that construct.
if (MSVC)
	target_compile_options (matrix_protocol PRIVATE /Zc:preprocessor)
endif ()
