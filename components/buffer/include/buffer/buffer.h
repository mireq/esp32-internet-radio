#pragma once

#include <stdbool.h>
#include <stdlib.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"


typedef struct buffer_t {
	char *buf;
	size_t size;
	size_t r_pos;
	size_t w_pos;
	bool destroying;
	SemaphoreHandle_t can_write_semaphore;
	SemaphoreHandle_t can_read_semaphore;
} buffer_t;


void buffer_init(buffer_t *buffer);
esp_err_t buffer_read(buffer_t *buffer, char *buf, size_t size);
esp_err_t buffer_write(buffer_t *buffer, char *buf, size_t size);
void buffer_clear(buffer_t *buffer);
void buffer_destroy(buffer_t *buffer);
size_t buffer_get_free(buffer_t *buffer);
size_t buffer_get_full(buffer_t *buffer);
