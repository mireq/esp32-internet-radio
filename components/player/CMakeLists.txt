set(COMPONENT_SRCS
	"audio_output.c"
	"control_http.c"
	"decoder.c"
	"events.c"
	"player.c"
	"source.c"
)
set(COMPONENT_ADD_INCLUDEDIRS "include")
set(COMPONENT_REQUIRES "http_header_parser" "http_server" "buffer" "mad" "mbedtls")
register_component()
