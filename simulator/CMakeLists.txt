cmake_minimum_required(VERSION 3.5)

project(simulator)

include_directories(
	"${CMAKE_SOURCE_DIR}/include/"
	"${CMAKE_SOURCE_DIR}/../libs/Posix_GCC_Simulator/FreeRTOS_Posix"
	"${CMAKE_SOURCE_DIR}/../libs/Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/include"
	"${CMAKE_SOURCE_DIR}/../libs/Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/portable/GCC/Posix"
)

add_executable(
	${PROJECT_NAME}
	"${CMAKE_SOURCE_DIR}/../libs/Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/croutine.c"
	"${CMAKE_SOURCE_DIR}/../libs/Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/list.c"
	"${CMAKE_SOURCE_DIR}/../libs/Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/queue.c"
	"${CMAKE_SOURCE_DIR}/../libs/Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/tasks.c"
	"${CMAKE_SOURCE_DIR}/../libs/Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/portable/GCC/Posix/port.c"
	"${CMAKE_SOURCE_DIR}/../libs/Posix_GCC_Simulator/FreeRTOS_Posix/FreeRTOS_Kernel/portable/MemMang/heap_3.c"
	startup.c
	"${CMAKE_SOURCE_DIR}/../main/main.c"
)
TARGET_LINK_LIBRARIES(
	${PROJECT_NAME}
	pthread
)
