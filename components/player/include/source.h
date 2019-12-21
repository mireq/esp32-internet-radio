#pragma once

#include <stdlib.h>

#define MAX_URI_SIZE 250


struct source_t;


typedef enum source_status {
	SOURCE_STATUS_OPENING,
	SOURCE_STATUS_OPEN,
	SOURCE_STATUS_CLOSED,
} source_status;


typedef enum source_error {
	SOURCE_NO_ERROR = 0,
	SOURCE_URI_ERROR,
	SOURCE_READING_ERROR,
} source_error;


typedef enum source_type {
	SOURCE_TYPE_HTTP,
} source_type;


typedef void (*source_callback) (struct source_t*);
typedef void (*source_data_callback) (struct source_t*, const char *, size_t);
typedef void (*source_status_callback) (struct source_t*, source_status);


typedef struct source_config {
	source_data_callback on_data;
	source_status_callback on_status;
} source_config;


typedef struct uri_t {
	char uri[MAX_URI_SIZE + 3];
	char *protocol;
	char *host;
	int port;
	char *path;
} uri_t;


typedef struct source_data_http {
	int icy_meta_interval;
} source_data_http;


typedef union source_data {
	source_data_http http;
} source_data;


typedef struct source_t {
	void *handle; // Any handle
	struct source_config config;
	source_type type;
	source_data data;
} source_t;


source_error source_init(source_t *source, const char *uri, source_config callback);
void source_destroy(source_t *source);
