project (spellchecker)

cmake_minimum_required (VERSION 3.16)

find_package (Kadu REQUIRED CONFIG)

set (SOURCES
	configuration/spellchecker-configuration.cpp
	highlighter.cpp
	spellchecker-configuration-ui-handler.cpp
	spellchecker-module.cpp
	spellchecker-plugin-modules-factory.cpp
	spellchecker-plugin-object.cpp
	spellchecker.cpp
	suggest-action.cpp
	suggester.cpp
)

set (CONFIGURATION_FILES
	configuration/spellchecker.ui
)

if (APPLE)
	list (APPEND SOURCES macspellchecker.mm)
	list (APPEND MOC_SOURCES macspellchecker.h)
else ()
	if (WITH_ASPELL AND NOT WITH_ENCHANT)
		set (USE_ASPELL TRUE)
		set (FORCE_ENGINE TRUE)
	else ()
		set (USE_ENCHANT TRUE)
		if (WITH_ENCHANT)
			set (FORCE_ENGINE TRUE)
		endif ()

		if (WITH_ASPELL)
			message (STATUS "Cannot use both enchant and aspell. Using enchant")
		endif ()
	endif ()

	if (USE_ENCHANT)
		find_package (PkgConfig)
		# enchant 2.x ships its pkg-config file as enchant-2; the old name only exists on
		# systems still carrying the 1.x series.
		pkg_search_module (ENCHANT enchant-2 enchant)

		if (NOT ENCHANT_FOUND)
			if (FORCE_ENGINE)
				message (SEND_ERROR "enchant not found!")
			else ()
				message (STATUS "enchant not found. Falling back to aspell")
				set (USE_ENCHANT FALSE)
				set (USE_ASPELL TRUE)
			endif ()
		endif ()
	endif ()

	if (USE_ASPELL)
		find_package (aspell CONFIG QUIET)

		if (TARGET aspell::aspell)
			set (ASPELL_FOUND TRUE)
			set (ASPELL_LIBRARIES aspell::aspell)
		else ()
			find_path (ASPELL_INCLUDE_DIR aspell.h)
			find_library (ASPELL_LIBRARY NAMES aspell libaspell)
			include (FindPackageHandleStandardArgs)
			find_package_handle_standard_args (ASPELL REQUIRED_VARS ASPELL_INCLUDE_DIR ASPELL_LIBRARY)
			set (ASPELL_LIBRARIES ${ASPELL_LIBRARY})
		endif ()

		if (NOT ASPELL_FOUND)
			message (SEND_ERROR "aspell not found!")
		endif ()
	endif ()

	if (USE_ENCHANT)
		message (STATUS "Using enchant as backend. To force aspell use -DWITH_ASPELL=ON")
		add_definitions (-DHAVE_ENCHANT)

		set (LIBRARIES ${ENCHANT_LIBRARIES})
		link_directories (${ENCHANT_LIBRARY_DIRS})
		include_directories (${ENCHANT_INCLUDE_DIRS})
		add_definitions (${ENCHANT_DEFINITIONS})
	else ()
		message (STATUS "Using aspell as backend")
		add_definitions (-DHAVE_ASPELL)

		set (LIBRARIES ${ASPELL_LIBRARIES})
		if (NOT TARGET aspell::aspell)
			include_directories (${ASPELL_INCLUDE_DIR})
		endif ()
	endif ()
endif ()

kadu_plugin (spellchecker
	PLUGIN_SOURCES ${SOURCES}
	PLUGIN_CONFIGURATION_FILES ${CONFIGURATION_FILES}
	PLUGIN_LIBRARIES ${LIBRARIES}
)
