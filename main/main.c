#include <stdio.h>
#include "source.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


void exit_app() {
	vTaskEndScheduler();
}


void on_stream_data(struct stream_t *stream, const char *data, size_t length) {

}

void on_stream_status(struct stream_t *stream, enum stream_status status) {

}


void app_main() {
	stream_config callbacks = {
		.on_data = NULL,
		.on_status = NULL,
	};
	stream_t *stream = stream_init("http://ice1.somafm.com/illstreet-128-mp3", callbacks);
	if (!stream) {
		exit_app();
	}
	printf("Hello world!\n");
	for (int i = 3; i >= 0; i--) {
		printf("Restarting in %d seconds...\n", i);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
	stream_destroy(stream);
	exit_app();
}
