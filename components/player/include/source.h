#pragma once

#include <stdlib.h>


struct stream_t;


typedef enum stream_status {
	STREAM_STATUS_OPEN,
	STREAM_STATUS_CLOSE,
} stream_status;


typedef enum stream_type {
	STREAM_TYPE_HTTP,
} stream_type;


typedef void (*stream_callback) (struct stream_t*);
typedef void (*stream_data_callback) (struct stream_t*, const char *, size_t);
typedef void (*stream_status_callback) (struct stream_t*, stream_status);


typedef struct stream_config {
	stream_data_callback on_data;
	stream_status_callback on_status;
} stream_config;


typedef struct stream_t {
	void *handle;
	struct stream_config config;
	stream_type type;
} stream_t;

stream_t *stream_init(const char *uri, stream_config callback);
void stream_destroy(stream_t *stream);
