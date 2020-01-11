set(MAD_SOURCES
	"bit.c"
	"decoder.c"
	"fixed.c"
	"frame.c"
	"huffman.c"
	"layer12.c"
	"layer3.c"
	"stream.c"
	"synth.c"
	"timer.c"
	"version.c"
)
include_directories(
	"${CMAKE_CURRENT_SOURCE_DIR}/src/"
)
list(TRANSFORM MAD_SOURCES PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/src/)
add_definitions(-DHAVE_CONFIG_H -g3 -ggdb)
add_library(mad STATIC
	${MAD_SOURCES}
)
