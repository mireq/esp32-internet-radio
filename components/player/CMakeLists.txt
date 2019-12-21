set(COMPONENT_SRCS
	"player.c"
	"events.c"
	"source.c"
)
set(COMPONENT_ADD_INCLUDEDIRS "include")
set(COMPONENT_REQUIRES "http_header_parser")
register_component()
