cmake_minimum_required(VERSION 3.0)
project(IsoMateric)

# set glfw parameters
option(GLFW_BUILD_DOCS OFF)
option(GLFW_BUILD_EXAMPLES OFF)
option(GLFW_BUILD_TESTS OFF)
add_subdirectory(IsoMateric/vendor/glfw)

# set assimp parameters
option(ASSIMP_BUILD_ASSIMP_TOOLS OFF)
option(ASSIMP_BUILD_SAMPLES OFF)
option(ASSIMP_BUILD_TESTS OFF)
add_subdirectory(IsoMateric/vendor/assimp)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -std=c++11 -lfreetype")
set(GLAD_LIBRARIES dl)

# include directories
include_directories(
	IsoMateric/vendor/assimp/include/
        IsoMateric/vendor/glad/include/
        IsoMateric/vendor/glfw/include/
        IsoMateric/vendor/glm/
        IsoMateric/vendor/stb/
	IsoMateric/vendor/imgui/
	IsoMateric/vendor/imgui/backends
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
	IsoMateric/src/camera.cpp

	# imgui
	IsoMateric/vendor/imgui/imgui.cpp
  	IsoMateric/vendor/imgui/imgui_demo.cpp
  	IsoMateric/vendor/imgui/imgui_draw.cpp
  	IsoMateric/vendor/imgui/imgui_tables.cpp
	IsoMateric/vendor/imgui/imgui_widgets.cpp
	IsoMateric/vendor/imgui/backends/imgui_impl_opengl3.cpp
	IsoMateric/vendor/imgui/backends/imgui_impl_glfw.cpp
	
	)


# header files to take into account
set(
	HEADER_EXECUTABLES
	IsoMateric/src/isomateric.h
	IsoMateric/src/renderer.h
	IsoMateric/src/resourceManager.h
	IsoMateric/src/shader.h
	IsoMateric/src/texture.h
	IsoMateric/src/camera.h
	)

# join everything
add_executable(IsoMateric IsoMateric/vendor/glad/src/glad.c ${SOURCE_EXECUTABLES} ${HEADER_EXECUTABLES})

# set libraries (if using at all)
target_link_libraries(
	IsoMateric 
	assimp 
	glfw
	${GLFW_LIBRARIES}
	${GLAD_LIBRARIES}
	)

# set target properties
set_target_properties(IsoMateric PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/IsoMateric/bin")


