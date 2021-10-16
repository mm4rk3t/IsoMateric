cmake_minimum_required(VERSION 3.0)
project(IsoMateric)

# set glfw parameters
option(GLFW_BUILD_DOCS OFF)
option(GLFW_BUILD_EXAMPLES OFF)
option(GLFW_BUILD_TESTS OFF)
add_subdirectory(IsoMateric/vendor/glfw)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -std=c++11 -lfreetype")
set(GLAD_LIBRARIES dl)

# include directories
include_directories(
        IsoMateric/vendor/glad/include/
        IsoMateric/vendor/glfw/include/
        IsoMateric/vendor/glm/
        IsoMateric/vendor/stb/
	/usr/include/freetype2
	)


# source files to compile
set(
	SOURCE_EXECUTABLES
	IsoMateric/src/isomateric.cpp
	IsoMateric/src/application.cpp
	IsoMateric/src/renderer.cpp
	IsoMateric/src/resourceManager.cpp
	IsoMateric/src/shader.cpp
	IsoMateric/src/texture.cpp
	)


# header files to take into account
set(
	HEADER_EXECUTABLES
	IsoMateric/src/isomateric.h
	IsoMateric/src/renderer.h
	IsoMateric/src/resourceManager.h
	IsoMateric/src/shader.h
	IsoMateric/src/texture.h
	)

# join everything
add_executable(IsoMateric IsoMateric/vendor/glad/src/glad.c ${SOURCE_EXECUTABLES} ${HEADER_EXECUTABLES})

# set libraries (if using at all)
target_link_libraries(
	IsoMateric 
	glfw
	${GLFW_LIBRARIES}
	${GLAD_LIBRARIES}
	)

# set target properties
set_target_properties(IsoMateric PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/IsoMateric/bin")


