set(COMPONENT_SRCS
	"audio_output.c"
	"bits.c"
	"control_http.c"
	"decoder.c"
	"interface.c"
	"player.c"
	"playlist.c"
	"source.c"
)
set(COMPONENT_ADD_INCLUDEDIRS "include")
set(COMPONENT_REQUIRES "http_header_parser" "http_server" "buffer" "mad" "mbedtls")
register_component()
