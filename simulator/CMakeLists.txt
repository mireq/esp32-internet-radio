cmake_minimum_required(VERSION 3.5)

project(simulator)

include_directories(
	"${CMAKE_SOURCE_DIR}/include/"
	"${CMAKE_SOURCE_DIR}/../libs/Posix_GCC_Simulator/FreeRTOS_Posix"
	"${CMAKE_SOURCE_DIR}/../libs/Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/include"
	"${CMAKE_SOURCE_DIR}/../libs/Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/portable/GCC/Posix"
	"${CMAKE_SOURCE_DIR}/../components/buffer/include/"
	"${CMAKE_SOURCE_DIR}/../components/http_header_parser/include/"
	"${CMAKE_SOURCE_DIR}/../components/player/include/"
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
	esp_err.c
	esp_event.c
	esp_heap_caps.c
	esp_log.c
	init.c
	startup.c
	"${CMAKE_SOURCE_DIR}/../libs/Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/croutine.c"
	"${CMAKE_SOURCE_DIR}/../libs/Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/list.c"
	"${CMAKE_SOURCE_DIR}/../libs/Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/queue.c"
	"${CMAKE_SOURCE_DIR}/../libs/Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/tasks.c"
	"${CMAKE_SOURCE_DIR}/../libs/Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/portable/GCC/Posix/port.c"
	"${CMAKE_SOURCE_DIR}/../libs/Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/portable/MemMang/heap_3.c"
	"$ENV{IDF_PATH}/components/esp_event/esp_event.c"
	"${CMAKE_SOURCE_DIR}/../components/buffer/buffer.c"
	"${CMAKE_SOURCE_DIR}/../components/http_header_parser/http_header_parser.c"
	"${CMAKE_SOURCE_DIR}/../components/player/audio_output.c"
	"${CMAKE_SOURCE_DIR}/../components/player/control.c"
	"${CMAKE_SOURCE_DIR}/../components/player/decoder.c"
	"${CMAKE_SOURCE_DIR}/../components/player/events.c"
	"${CMAKE_SOURCE_DIR}/../components/player/player.c"
	"${CMAKE_SOURCE_DIR}/../components/player/source.c"
	"${CMAKE_SOURCE_DIR}/../main/init.c"
	"${CMAKE_SOURCE_DIR}/../main/main.c"
)

add_definitions(-D_GNU_SOURCE -DSIMULATOR -g3 -ggdb)

target_link_libraries(
	${PROJECT_NAME}
	pthread
	asound
	mad
)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 11)
