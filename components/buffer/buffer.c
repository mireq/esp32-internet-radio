#include <buffer/buffer.h>

#include "esp_log.h"


static const char *TAG = "buffer";


void buffer_init(buffer_t *buffer) {
	buffer->can_write_semaphore = NULL;
	buffer->can_read_semaphore = NULL;
	buffer->r_pos = 0;
	buffer->w_pos = 0;
	buffer->eof = true;
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
			if (buffer->eof) {
				return ESP_FAIL;
			}
			xSemaphoreTake(buffer->can_read_semaphore, portMAX_DELAY);
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


void buffer_open(buffer_t *buffer) {
	buffer->eof = false;
	buffer->r_pos = 0;
	buffer->w_pos = 0;
	xSemaphoreGive(buffer->can_write_semaphore);
	xSemaphoreGive(buffer->can_read_semaphore);
}


void buffer_set_eof(buffer_t *buffer) {
	buffer->eof = true;
	xSemaphoreGive(buffer->can_write_semaphore);
	xSemaphoreGive(buffer->can_read_semaphore);
}


esp_err_t buffer_write(buffer_t *buffer, char *buf, size_t size) {
	if (buffer->eof) {
		return ESP_FAIL;
	}
	size_t pos = 0;
	while (pos < size) {
		size_t r_pos = buffer->r_pos;
		size_t w_pos = buffer->w_pos;
		if (buffer->w_pos + 1 == r_pos || (r_pos == 0 && buffer->w_pos == buffer->size - 1)) {
			xSemaphoreTake(buffer->can_write_semaphore, portMAX_DELAY);
			if (buffer->eof) {
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
	buffer->eof = true;
	buffer->r_pos = 0;
	buffer->w_pos = 0;
	xSemaphoreGive(buffer->can_write_semaphore);
	xSemaphoreGive(buffer->can_read_semaphore);
}


void buffer_destroy(buffer_t *buffer) {
	buffer_clear(buffer);
	buffer->eof = true;
	buffer->r_pos = 0;
	buffer->w_pos = 0;
	vSemaphoreDelete(buffer->can_write_semaphore);
	vSemaphoreDelete(buffer->can_read_semaphore);
	buffer->can_write_semaphore = NULL;
	buffer->can_read_semaphore = NULL;
}


size_t buffer_get_full(buffer_t *buffer) {
	size_t r_pos = buffer->r_pos;
	size_t w_pos = buffer->w_pos;
	size_t size = buffer->size;
	if (size == 0) {
		return 0;
	}
	if (w_pos >= r_pos) {
		return w_pos - r_pos;
	}
	else {
		return size - r_pos + w_pos;
	}
}


size_t buffer_get_free(buffer_t *buffer) {
	return buffer->size - buffer_get_full(buffer);
}
