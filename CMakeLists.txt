# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32-radio)

if (CONFIG_BUILD_SIMULATOR)
	idf_build_get_property(config_dir CONFIG_DIR)
	set(sdkconfig_header ${config_dir}/sdkconfig.h)
	set(project_elf ${CMAKE_PROJECT_NAME}.elf)
	ExternalProject_Add(
		linux
		SOURCE_DIR "${CMAKE_SOURCE_DIR}/linux"
		INSTALL_COMMAND ""
		TEST_COMMAND ""
		BUILD_ALWAYS TRUE
		DEPENDS ${project_elf}
		CMAKE_ARGS -DCMAKE_ROOT_BUILD_DIR=${CMAKE_BINARY_DIR} -DESP_CONFIG_DIR=${config_dir}
	)
endif ()
