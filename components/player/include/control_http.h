#pragma once

#include "sdkconfig.h"


#if CONFIG_HTTP_CONTROL


/* Command codes */
typedef enum WS_COMMAND_t {
	WS_COMMAND_PING,
	WS_COMMAND_SET_PLAYLIST_ITEM,
	WS_COMMAND_SET_VOLUME,
} WS_COMMAND_t;


/* Response codes */
typedef enum WS_RESPONSE_t {
	WS_RESPONSE_PONG,
	WS_RESPONSE_STATUS,
} WS_RESPONSE_t;


typedef enum WS_STATUS_t {
	WS_STATUS_VOLUME,
} WS_STATUS_t;


void http_control_init(void);


#endif
