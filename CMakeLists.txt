PROJECT(svpn)
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
SET(PROJECT_ROOT_PATH "${CMAKE_SOURCE_DIR}")
SET(EXECUTABLE_OUTPUT_PATH "${PROJECT_ROOT_PATH}/bin")
SET(LIBRARY_OUTPUT_PATH "${PROJECT_ROOT_PATH}/lib")
SET(GCC_COVERAGE_COMPILE_FLAGS "-lz -lm")
SET(GCC_COVERAGE_LINK_FLAGS    "-lz -lm")
SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS}" )
SET( CMAKE_C_FLAGS  "${CMAKE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS}" )
SET( CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${GCC_COVERAGE_LINK_FLAGS}" )
INCLUDE_DIRECTORIES("${PROJECT_ROOT_PATH}/include")
LINK_DIRECTORIES("${PROJECT_ROOT_PATH}/lib")
ADD_SUBDIRECTORY(src)
