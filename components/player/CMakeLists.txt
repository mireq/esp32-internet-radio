set(COMPONENT_SRCS
	"audio_output.c"
	"control.c"
	"decoder.c"
	"events.c"
	"player.c"
	"source.c"
)
set(COMPONENT_ADD_INCLUDEDIRS "include")
set(COMPONENT_REQUIRES "http_header_parser" "buffer" "mad")
register_component()
