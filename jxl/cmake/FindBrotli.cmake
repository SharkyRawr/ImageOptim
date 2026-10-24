# Keep Brotli independently pinned without modifying the libjxl submodule.
set(BROTLI_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../brotli" "${CMAKE_BINARY_DIR}/brotli")
set(Brotli_FOUND TRUE)
