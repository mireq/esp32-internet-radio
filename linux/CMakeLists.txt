cmake_minimum_required(VERSION 3.5)

project(linux)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../simulator/src ${CMAKE_CURRENT_BINARY_DIR}/simulator)
add_subdirectory("mad")

include_directories(
	${ESP_SIMULATOR_INCLUDE_DIRECTORIES}
	"${CMAKE_SOURCE_DIR}/../components/buffer/include/"
	"${CMAKE_SOURCE_DIR}/../components/http_header_parser/include/"
	"${CMAKE_SOURCE_DIR}/../components/http_server/include/"
	"${CMAKE_SOURCE_DIR}/../components/player/include/"
	"${CMAKE_SOURCE_DIR}/../components/mad/include/"
	"${CMAKE_SOURCE_DIR}/../components/"
	"${CMAKE_SOURCE_DIR}/../main/include/"
	"$ENV{IDF_PATH}/components/log/include/"
	"$ENV{IDF_PATH}/components/esp_common/include/"
	"$ENV{IDF_PATH}/components/esp_event/include/"
	"$ENV{IDF_PATH}/components/"
	"${CMAKE_CURRENT_BINARY_DIR}/../../../config/"
)

add_executable(
	${PROJECT_NAME}
	init.c

	"${CMAKE_SOURCE_DIR}/../components/buffer/buffer.c"
	"${CMAKE_SOURCE_DIR}/../components/http_header_parser/http_header_parser.c"
	"${CMAKE_SOURCE_DIR}/../components/http_server/http_server.c"
	"${CMAKE_SOURCE_DIR}/../components/player/audio_output.c"
	"${CMAKE_SOURCE_DIR}/../components/player/bits.c"
	"${CMAKE_SOURCE_DIR}/../components/player/control_http.c"
	"${CMAKE_SOURCE_DIR}/../components/player/decoder.c"
	"${CMAKE_SOURCE_DIR}/../components/player/interface.c"
	"${CMAKE_SOURCE_DIR}/../components/player/player.c"
	"${CMAKE_SOURCE_DIR}/../components/player/playlist.c"
	"${CMAKE_SOURCE_DIR}/../components/player/source.c"
	"${CMAKE_SOURCE_DIR}/../main/init.c"
	"${CMAKE_SOURCE_DIR}/../main/main.c"
	"${CMAKE_SOURCE_DIR}/../main/trace.c"
	"${CMAKE_SOURCE_DIR}/../main/task_debug.c"
)

add_definitions(-D_GNU_SOURCE -DSIMULATOR -g3 -ggdb)

target_link_libraries(
	${PROJECT_NAME}
	asound
	mad
	simulator
)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 11)
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_ROOT_BUILD_DIR}")
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "linux_simulator")
