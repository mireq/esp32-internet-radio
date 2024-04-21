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

idf_component_register(SRCS ${COMPONENT_SRCS} INCLUDE_DIRS "include" REQUIRES "http_header_parser" "http_server" "buffer" "mad" "mbedtls" "esp_event" "driver")
