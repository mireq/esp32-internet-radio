#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "http_header_parser/http_header_parser.h"


void http_header_parser_init(http_header_parser_t *parser, header_parser_callback callback, void *handle) {
	parser->handle = handle;
	parser->status = -1;
	bzero(parser->key, sizeof(parser->key));
	parser->key_length = -1;
	bzero(parser->value, sizeof(parser->value));
	parser->value_length = 0;
	parser->header_finished = 0;
	parser->header_error = 0;
	parser->callback = callback;
	parser->last_c = '\0';
	return parser;
}

void http_header_parser_destroy(http_header_parser_t *parser) {
}

void http_header_parser_feed(http_header_parser_t *parser, char c) {
	if (parser->header_finished || parser->header_error) {
		return;
	}
	if (c == '\n' && parser->last_c == '\r') {
		// Header
		if (parser->value_length != -1 && parser->key_length == -1) {
			parser->status = atoi(parser->value + sizeof("HTTP/1.0 ") - 1);
			parser->key_length = -1;
			parser->value_length = -1;
			bzero(parser->key, sizeof(parser->key));
			bzero(parser->value, sizeof(parser->value));
			parser->callback(parser);
		}
		else if (parser->key_length == -1 && parser->value_length == -1) {
			parser->header_finished = 1;
			parser->callback(parser);
		}
		else {
			parser->callback(parser);
			bzero(parser->key, sizeof(parser->key));
			bzero(parser->value, sizeof(parser->value));
			parser->key_length = -1;
			parser->value_length = -1;
		}
		parser->last_c = c;
		return;
	}

	if (c != '\n' && c != '\r') {
		// Reading value
		if (parser->value_length != -1) {
			if (parser->value_length != 0 || c != ' ') { // Skip whitespace before value
				if (parser->value_length < sizeof(parser->key) - 1) {
					parser->value[parser->value_length] = c;
				}
				parser->value_length++;
			}
		}
		// Reading key
		else {
			if (parser->key_length == -1) {
				parser->key_length = 0;
			}
			if (c == ':') {
				parser->value_length = 0;
			}
			else {
				if (parser->key_length < sizeof(parser->key) - 1) {
					parser->key[parser->key_length] = c;
				}
				parser->key_length++;
			}
		}
	}
	parser->last_c = c;
}
