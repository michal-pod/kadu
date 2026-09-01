find_package (Python3 REQUIRED COMPONENTS Interpreter)

set (emoji_catalogue_directory "${CMAKE_CURRENT_BINARY_DIR}/generated")
set (emoji_catalogue_source "${emoji_catalogue_directory}/emoji-catalog-generated.cpp")
set (emoji_catalogue_header "${emoji_catalogue_directory}/emoji-catalog-generated.h")

add_custom_command (
	OUTPUT "${emoji_catalogue_source}"
	BYPRODUCTS "${emoji_catalogue_header}"
	COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/varia/emoji/generate-emoji-catalog.py"
		--source "${CMAKE_SOURCE_DIR}/varia/emoji/emoji-test.txt"
		--format cpp
		--output "${emoji_catalogue_source}"
		--header "${emoji_catalogue_header}"
	DEPENDS
		"${CMAKE_SOURCE_DIR}/varia/emoji/generate-emoji-catalog.py"
		"${CMAKE_SOURCE_DIR}/varia/emoji/emoji-test.txt"
	VERBATIM
)

set (emoji_SRCS
	emoji-font.cpp
	emoji-font.h
	emoji-model.cpp
	emoji-model.h
	"${emoji_catalogue_source}"
)

kadu_subdirectory (emoji "${emoji_SRCS}")

# The generated catalogue header is private to this component. Normal linking
# collects this directory in the parent; FAST_LINKING builds the same source
# list as the standard `emoji` static target.
if (FAST_LINKING)
	target_include_directories (emoji PRIVATE "${emoji_catalogue_directory}")
else ()
	set (emoji_INCLUDES "${emoji_catalogue_directory}" PARENT_SCOPE)
endif ()
