#include <buffer/buffer.h>

#include "esp_log.h"


static const char *TAG = "buffer";


void buffer_init(buffer_t *buffer) {
	buffer->can_write_semaphore = NULL;
	buffer->can_read_semaphore = NULL;
	buffer->r_pos = 0;
	buffer->w_pos = 0;
	buffer->destroying = false;
	buffer->can_write_semaphore = xSemaphoreCreateBinary();
	if (!buffer->can_write_semaphore) {
		ESP_LOGE(TAG, "semaphore can_write_semaphore not created");
		abort();
	}
	buffer->can_read_semaphore = xSemaphoreCreateBinary();
	if (!buffer->can_read_semaphore) {
		ESP_LOGE(TAG, "semaphore can_read_semaphore not created");
		abort();
	}
	xSemaphoreGive(buffer->can_write_semaphore);
	xSemaphoreGive(buffer->can_read_semaphore);
}


esp_err_t buffer_read(buffer_t *buffer, char *buf, size_t size) {
	size_t pos = 0;
	while (pos < size) {
		size_t r_pos = buffer->r_pos;
		size_t w_pos = buffer->w_pos;
		if (buffer->r_pos == w_pos) {
			xSemaphoreTake(buffer->can_read_semaphore, portMAX_DELAY);
			if (buffer->destroying) {
				return ESP_FAIL;
			}
		}
		else {
			buf[pos] = buffer->buf[buffer->r_pos];
			pos++;
			r_pos++;
			if (r_pos >= buffer->size) {
				r_pos = 0;
			}
			buffer->r_pos = r_pos;
		}
	}
	xSemaphoreGive(buffer->can_write_semaphore);
	return ESP_OK;
}


esp_err_t buffer_write(buffer_t *buffer, char *buf, size_t size) {
	size_t pos = 0;
	while (pos < size) {
		size_t r_pos = buffer->r_pos;
		size_t w_pos = buffer->w_pos;
		if (buffer->w_pos + 1 == r_pos || (r_pos == 0 && buffer->w_pos == buffer->size - 1)) {
			xSemaphoreTake(buffer->can_write_semaphore, portMAX_DELAY);
			if (buffer->destroying) {
				return ESP_FAIL;
			}
		}
		else {
			buffer->buf[buffer->w_pos] = buf[pos];
			pos++;
			w_pos++;
			if (w_pos >= buffer->size) {
				w_pos = 0;
			}
			buffer->w_pos = w_pos;
		}
	}
	xSemaphoreGive(buffer->can_read_semaphore);
	return ESP_OK;
}


void buffer_clear(buffer_t *buffer) {
	buffer->destroying = true;
	xSemaphoreGive(buffer->can_write_semaphore);
	xSemaphoreGive(buffer->can_read_semaphore);
	buffer->r_pos = 0;
	buffer->w_pos = 0;
	buffer->destroying = false;
}


void buffer_destroy(buffer_t *buffer) {
	buffer_clear(buffer);
	buffer->destroying = true;
	vSemaphoreDelete(buffer->can_write_semaphore);
	vSemaphoreDelete(buffer->can_read_semaphore);
	buffer->can_write_semaphore = NULL;
	buffer->can_read_semaphore = NULL;
	buffer->destroying = false;
}
