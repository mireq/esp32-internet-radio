#pragma once

include <stdbool.h>
#include <stdlib.h>


typedef struct buffer_t {
	char *buf;
	size_t size;
	size_t r_pos;
	size_t w_pos;
} buffer_t;


void buffer_init(buffer_t *buffer);
bool buffer_read(buffer_t *buffer, char *buf, size_t size);
bool buffer_write(buffer_t *buffer, char *buf, size_t size);
void buffer_destroy(buffer_t *buffer);
