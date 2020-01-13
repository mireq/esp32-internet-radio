#pragma once

#include "sdkconfig.h"


#if CONFIG_HTTP_CONTROL


/* Command codes */
typedef enum WS_COMMAND_t {
	WS_COMMAND_PING,
	WS_COMMAND_SET_PLAYLIST_ITEM,
} WS_COMMAND_t;


/* Response codes */
typedef enum WS_RESPONSE_t {
	WS_RESPONSE_PONG,
} WS_RESPONSE_t;


void init_http_control(void);


#endif
