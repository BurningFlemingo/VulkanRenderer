cmake_minimum_required(VERSION 3.8)

set (PROJ_NAME VulkanRenderer)
project(${PROJ_NAME})
set(CMAKE_SUPPRESS_REGENERATION true)

find_package(SDL2 REQUIRED)
find_package(Stb REQUIRED)
find_package(tinyobjloader REQUIRED)
find_package(Vulkan REQUIRED COMPONENTS glslc)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

find_program(GLSLC NAMES glslc HINTS Vulkan::glslc)

set(CMAKE_CXX_STANDARD 23)

set(SHADERS_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/VulkanRenderer/src/shaders")
set(SHADERS_BIN_DIR "${CMAKE_BINARY_DIR}/shaders")

file(GLOB SHADERS
	"${SHADERS_SRC_DIR}/*.vert"
	"${SHADERS_SRC_DIR}/*.frag"
	"${SHADERS_SRC_DIR}/*.comp"
	"${SHADERS_SRC_DIR}/*.geom"
	"${SHADERS_SRC_DIR}/*.tesc"
	"${SHADERS_SRC_DIR}/*.tese")

file(MAKE_DIRECTORY "${SHADERS_BIN_DIR}")

foreach(SHADER ${SHADERS})
	get_filename_component(SHADER_NAME ${SHADER} NAME)
	set(SHADER_BIN_NAME "${SHADERS_BIN_DIR}/${SHADER_NAME}.spv")
	add_custom_command(
		MAIN_DEPENDENCY "${SHADER_BIN_DIR}"
		DEPENDS "${SHADER}"
		OUTPUT "${SHADER_BIN_NAME}"
		COMMAND "${GLSLC}" "${SHADER}" "-o" "${SHADER_BIN_NAME}"
		COMMENT "Compiling ${SHADER_NAME}"
		VERBATIM)
	list(APPEND SPV_SHADERS "${SHADER_BIN_NAME}")
endforeach()

add_custom_target(build_shaders DEPENDS ${SPV_SHADERS})

set (SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/VulkanRenderer/src")
set (VENDOR_DIR "${CMAKE_SOURCE_DIR}/vendor")

set(SRC_FILES
	${SRC_DIR}/Renderer.cpp
	${SRC_DIR}/Extensions.cpp
	${SRC_DIR}/Logger.cpp
	${SRC_DIR}/Instance.cpp
	${SRC_DIR}/Main.cpp
	${SRC_DIR}/DeletionQueue.cpp
	${SRC_DIR}/Device.cpp
	${SRC_DIR}/Swapchain.cpp
	${SRC_DIR}/Memory.cpp
	${SRC_DIR}/Commands.cpp
	)

set(DEBUG_FILES
	${SRC_DIR}/Layers.cpp
)
set(DEBUG_STUB_FILES
	${SRC_DIR}/stubs/Layers.cpp
)

if (CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
	set(SRC_FILES ${SRC_FILES} ${DEBUG_FILES})
else()
	set(SRC_FILES ${SRC_FILES} ${DEBUG_STUB_FILES})
endif()

add_executable(${PROJ_NAME} ${SRC_FILES})

target_link_libraries(${PROJ_NAME} PRIVATE SDL2::SDL2 SDL2::SDL2main Vulkan::Vulkan GPUOpen::VulkanMemoryAllocator imgui::imgui spdlog::spdlog_header_only)
target_include_directories(${PROJ_NAME} PRIVATE ${SRC_DIR} "${CMAKE_SOURCE_DIR}/vendor/")

if (CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
	target_compile_definitions(${PROJ_NAME}
		PRIVATE
		INTERNAL_BUILD
		LOG_LEVEL_WARNING
		PYX_BUILD_PYXENGINE
	)
else()
endif()

if (EXISTS ${CMAKE_BINARY_DIR}/compile_commands.json)
	add_custom_command(
		TARGET ${PROJ_NAME} POST_BUILD
	    COMMAND ${CMAKE_COMMAND} -E copy
	        "${CMAKE_BINARY_DIR}/compile_commands.json"
	        "${CMAKE_SOURCE_DIR}"
	    DEPENDS ${CMAKE_BINARY_DIR}/compile_commands.json
	    COMMENT "Copying compile_commands.json to src directory"
	    VERBATIM
	)
endif()

add_dependencies(${PROJ_NAME} build_shaders)
