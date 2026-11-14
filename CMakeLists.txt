cmake_minimum_required(VERSION 3.16)
project(IsoMateric C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

set(VENDOR ${CMAKE_SOURCE_DIR}/external)

# --- glfw -------------------------------------------------------------------
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
add_subdirectory(${VENDOR}/glfw)

# --- glad -------------------------------------------------------------------
add_library(glad STATIC ${VENDOR}/glad/src/glad.c)
target_include_directories(glad PUBLIC ${VENDOR}/glad/include)
target_link_libraries(glad PUBLIC ${CMAKE_DL_LIBS})

# --- imgui ------------------------------------------------------------------
add_library(imgui STATIC
	${VENDOR}/imgui/imgui.cpp
	${VENDOR}/imgui/imgui_draw.cpp
	${VENDOR}/imgui/imgui_tables.cpp
	${VENDOR}/imgui/imgui_widgets.cpp
	${VENDOR}/imgui/backends/imgui_impl_glfw.cpp
	${VENDOR}/imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${VENDOR}/imgui ${VENDOR}/imgui/backends)
target_link_libraries(imgui PUBLIC glfw)

# --- lua (compiled as C++ so script errors unwind C++ destructors) ------------
file(GLOB LUA_SOURCES ${VENDOR}/lua/*.c)
set_source_files_properties(${LUA_SOURCES} PROPERTIES LANGUAGE CXX)
add_library(lua STATIC ${LUA_SOURCES})
target_include_directories(lua PUBLIC ${VENDOR}/lua)
if(UNIX)
	target_compile_definitions(lua PRIVATE LUA_USE_LINUX)
	target_link_libraries(lua PUBLIC m ${CMAKE_DL_LIBS})
endif()

# --- miniaudio / cgltf / entt / tracy -------------------------------------
find_package(Threads REQUIRED)
add_library(miniaudio STATIC src/miniaudio_impl.c)
target_include_directories(miniaudio SYSTEM PUBLIC ${VENDOR}/miniaudio)
target_link_libraries(miniaudio PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
if(UNIX AND NOT APPLE)
	target_link_libraries(miniaudio PUBLIC m)
endif()

option(ISOMATERIC_TRACY "Enable the Tracy profiler client" OFF)
add_library(tracy_client INTERFACE)
target_include_directories(tracy_client SYSTEM INTERFACE ${VENDOR}/tracy/public)
if(ISOMATERIC_TRACY)
	add_library(tracy_impl STATIC ${VENDOR}/tracy/public/TracyClient.cpp)
	target_include_directories(tracy_impl SYSTEM PUBLIC ${VENDOR}/tracy/public)
	target_compile_definitions(tracy_impl PUBLIC TRACY_ENABLE)
	target_link_libraries(tracy_impl PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
	target_link_libraries(tracy_client INTERFACE tracy_impl)
endif()

# --- warnings for our own code ----------------------------------------------
function(isomateric_warnings target)
	if(MSVC)
		target_compile_options(${target} PRIVATE /W4)
	else()
		target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
	endif()
endfunction()

# --- core: game logic with no OpenGL dependency (unit-testable) ---------------
add_library(isomateric_core STATIC
	src/level.cpp
	src/player.cpp
	src/pathfinding.cpp
	src/particles.cpp
)
target_include_directories(isomateric_core PUBLIC src)
target_include_directories(isomateric_core SYSTEM PUBLIC ${VENDOR}/glm ${VENDOR}/entt)
target_compile_definitions(isomateric_core PUBLIC GLM_FORCE_SILENT_WARNINGS)
isomateric_warnings(isomateric_core)

# --- engine + game executable ---------------------------------------------------
add_executable(IsoMateric
	src/main.cpp
	src/isomateric.cpp
	src/input.cpp
	src/camera.cpp
	src/renderer.cpp
	src/shader.cpp
	src/texture.cpp
	src/model.cpp
	src/audio.cpp
	src/scripting.cpp
	src/profiler.cpp
	src/stb_impl.cpp
	src/cgltf_impl.cpp
)
# SYSTEM: third-party headers should not trigger our warnings
target_include_directories(IsoMateric SYSTEM PRIVATE ${VENDOR}/stb ${VENDOR}/cgltf)
target_link_libraries(IsoMateric PRIVATE isomateric_core glad glfw imgui lua miniaudio tracy_client)
target_compile_definitions(IsoMateric PRIVATE
	ISOMATERIC_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets"
	GLFW_INCLUDE_NONE # glad provides the GL headers
)
isomateric_warnings(IsoMateric)
set_target_properties(IsoMateric PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/IsoMateric/bin")

# --- tests ------------------------------------------------------------------
enable_testing()
add_executable(IsoMateric_tests tests/tests.cpp)
target_link_libraries(IsoMateric_tests PRIVATE isomateric_core)
isomateric_warnings(IsoMateric_tests)
set_target_properties(IsoMateric_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/IsoMateric/bin")
add_test(NAME core COMMAND IsoMateric_tests)
