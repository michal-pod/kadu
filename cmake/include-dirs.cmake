# Do not add the repository root to the global include path. On Windows its
# `VERSION` file is found case-insensitively for the C++20 standard header
# `<version>`, so the compiler tries to parse the release string as C++.
include_directories (${CMAKE_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/kadu-core)
