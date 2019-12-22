#pragma once

#include <stdlib.h>

#define MAX_URI_SIZE 250


struct source_t;


typedef enum source_status_t {
	SOURCE_STATUS_OPENING,
	SOURCE_STATUS_OPEN,
	SOURCE_STATUS_CLOSED,
} source_status_t;


typedef enum source_error_t {
	SOURCE_NO_ERROR = 0,
	SOURCE_URI_ERROR,
	SOURCE_READING_ERROR,
} source_error_t;


typedef enum source_type_t {
	SOURCE_TYPE_HTTP,
} source_type_t;


typedef void (*source_callback_t) (struct source_t*);
typedef void (*source_data_callback_t) (struct source_t*, const char *, size_t);
typedef void (*source_status_callback_t) (struct source_t*, source_status_t);


typedef struct source_config_t {
	source_data_callback_t on_data;
	source_status_callback_t on_status;
} source_config_t;


typedef struct uri_t {
	char uri[MAX_URI_SIZE + 3];
	char *protocol;
	char *host;
	int port;
	char *path;
} uri_t;


typedef struct source_data_http_t {
	int icy_meta_interval;
	int sock;
} source_data_http_t;


typedef struct source_t {
	void *handle; // Any handle
	struct source_config_t config;
	source_type_t type;
	union {
		source_data_http_t http;
	} data;
} source_t;


source_error_t source_init(source_t *source, const char *uri, source_config_t callback);
void source_destroy(source_t *source);
