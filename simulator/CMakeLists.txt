cmake_minimum_required(VERSION 3.5)

project(simulator)

include_directories(
	"${CMAKE_SOURCE_DIR}/include/"
	"${CMAKE_SOURCE_DIR}/../libs/Posix_GCC_Simulator/FreeRTOS_Posix"
	"${CMAKE_SOURCE_DIR}/../libs/Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/include"
	"${CMAKE_SOURCE_DIR}/../libs/Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/portable/GCC/Posix"
	"${CMAKE_SOURCE_DIR}/../components/player/include/"
	"${CMAKE_SOURCE_DIR}/../components/http_header_parser/include/"
	"${CMAKE_SOURCE_DIR}/../main/include/"
	"$ENV{IDF_PATH}/components/log/include/"
	"$ENV{IDF_PATH}/components/esp_common/include/"
	"$ENV{IDF_PATH}/components/"
	"${CMAKE_CURRENT_BINARY_DIR}/../../../config/"
)

add_executable(
	${PROJECT_NAME}
	init.c
	startup.c
	esp_log.c
	esp_err.c
	esp_event.c
	"${CMAKE_SOURCE_DIR}/../libs/Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/croutine.c"
	"${CMAKE_SOURCE_DIR}/../libs/Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/list.c"
	"${CMAKE_SOURCE_DIR}/../libs/Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/queue.c"
	"${CMAKE_SOURCE_DIR}/../libs/Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/tasks.c"
	"${CMAKE_SOURCE_DIR}/../libs/Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/portable/GCC/Posix/port.c"
	"${CMAKE_SOURCE_DIR}/../libs/Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/portable/MemMang/heap_3.c"
	"${CMAKE_SOURCE_DIR}/../components/http_header_parser/http_header_parser.c"
	"${CMAKE_SOURCE_DIR}/../components/player/source.c"
	"${CMAKE_SOURCE_DIR}/../main/events.c"
	"${CMAKE_SOURCE_DIR}/../main/main.c"
)

add_definitions(-DportTICK_PERIOD_MS=1 -D_GNU_SOURCE -DSIMULATOR)

target_link_libraries(
	${PROJECT_NAME}
	pthread
)
