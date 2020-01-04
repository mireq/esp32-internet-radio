#pragma once

#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>


#define HTTP_HEADER_KEY_BUFFER_SIZE 100
#define HTTP_HEADER_VALUE_BUFFER_SIZE 200


struct http_header_parser_t;

typedef void (*header_parser_callback) (struct http_header_parser_t*);

typedef struct http_header_parser_t {
	void *handle;
	int status;
	char key[HTTP_HEADER_KEY_BUFFER_SIZE];
	ssize_t key_length;
	char value[HTTP_HEADER_VALUE_BUFFER_SIZE];
	ssize_t value_length;
	bool header_finished;
	bool header_error;
	header_parser_callback callback;
	char last_c;
	bool request; // Parse request
} http_header_parser_t;


void http_header_parser_init(http_header_parser_t *parser, header_parser_callback callback, void *handle);
void http_header_parser_feed(http_header_parser_t *parser, char c);
