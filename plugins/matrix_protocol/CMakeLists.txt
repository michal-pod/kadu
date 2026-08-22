project (matrix_protocol)

cmake_minimum_required (VERSION 3.16)

find_package (Kadu REQUIRED CONFIG)
find_package (QuotientQt6 REQUIRED)

set (SOURCES
    matrix-account-data.cpp
    matrix-add-account-widget.cpp
    matrix-chat-service.cpp
    matrix-edit-account-widget.cpp
    matrix-id-validator.cpp
    matrix-module.cpp
    matrix-plugin-modules-factory.cpp
    matrix-plugin-object.cpp
    matrix-protocol-factory.cpp
    matrix-protocol.cpp
)

kadu_plugin (matrix_protocol
	PLUGIN_SOURCES ${SOURCES}
	PLUGIN_LIBRARIES QuotientQt6
)

# LibQuotient public headers use C++23 facilities (for example std::to_underlying).
# Keep the wider Kadu build at C++20; only this consumer needs the newer standard.
target_compile_features (matrix_protocol PRIVATE cxx_std_23)

# Quotient uses __VA_OPT__ in public event macros.  MSVC needs its
# conforming preprocessor explicitly enabled for that construct.
if (MSVC)
	target_compile_options (matrix_protocol PRIVATE /Zc:preprocessor)
endif ()
