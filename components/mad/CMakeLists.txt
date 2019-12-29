idf_component_register(
	SRCS
		"align.c"
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
	INCLUDE_DIRS "include"
)
target_compile_definitions(${COMPONENT_TARGET} PUBLIC "-DHAVE_CONFIG_H")
