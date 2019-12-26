set(COMPONENT_SRCS
	"player.c"
	"events.c"
	"source.c"
	"audio_output.c"
)
set(COMPONENT_ADD_INCLUDEDIRS "include")
set(COMPONENT_REQUIRES "http_header_parser" "buffer")
register_component()
