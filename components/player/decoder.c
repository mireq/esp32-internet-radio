#include <string.h>

#include "esp_log.h"

#include "decoder.h"


static const char *TAG = "decoder";


/* MAD */

static esp_err_t decoder_mpeg_init(decoder_t *decoder, source_t *source) {
	decoder_data_mpeg_t *mpeg = &decoder->data.mpeg;
	mad_stream_init(&mpeg->mad_stream);
	mad_synth_init(&mpeg->mad_synth);
	mad_frame_init(&mpeg->mad_frame);
	decoder->type = DECODER_MPEG;
	return ESP_OK;
}


static esp_err_t decoder_mpeg_feed(decoder_t *decoder, char *buf, ssize_t size) {
	decoder_data_mpeg_t *mpeg = &decoder->data.mpeg;
	mad_stream_buffer(&mpeg->mad_stream, buf, size);
	while (1) {
		if (mad_frame_decode(&mpeg->mad_frame, &mpeg->mad_stream)) {
			if (!MAD_RECOVERABLE(mpeg->mad_stream.error)) {
				return ESP_FAIL;
			}
			else if (mpeg->mad_stream.error == MAD_ERROR_BUFLEN) {
				// waiting for more data
				return ESP_OK;
			}
			else {
				return ESP_FAIL;
			}
		}
		mad_synth_frame(&mpeg->mad_synth, &mpeg->mad_frame);
		printf("synthetised frame\n");
	}
}


static void decoder_mpeg_destroy(decoder_t *decoder) {
	decoder->type = DECODER_UNKNOWN;
	decoder_data_mpeg_t *mpeg = &decoder->data.mpeg;
	mad_synth_finish(&mpeg->mad_synth);
	mad_frame_finish(&mpeg->mad_frame);
	mad_stream_finish(&mpeg->mad_stream);
}


/* Generic */


esp_err_t decoder_init(decoder_t *decoder, source_t *source) {
	decoder->type = DECODER_UNKNOWN;
	if (strcmp(source->content_type, "audio/mpeg") == 0) {
		return decoder_mpeg_init(decoder, source);
	}

	ESP_LOGW(TAG, "cannot decode content-type %s", source->content_type);
	return ESP_FAIL;
}


esp_err_t decoder_feed(decoder_t *decoder, char *buf, ssize_t size) {
	if (decoder->type == DECODER_MPEG) {
		return decoder_mpeg_feed(decoder, buf, size);
	}
	return ESP_FAIL;
}


void decoder_destroy(decoder_t *decoder) {
	if (decoder->type == DECODER_MPEG) {
		decoder_mpeg_destroy(decoder);
	}
	decoder->type = DECODER_UNKNOWN;
}
