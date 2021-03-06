#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "decoder.h"


static const char *TAG = "decoder";


/* MAD */

static esp_err_t decoder_mpeg_init(decoder_t *decoder, source_t *source) {
	decoder_data_mpeg_t *mpeg = &decoder->data.mpeg;
	mad_stream_init(&mpeg->mad_stream);
	mad_synth_init(&mpeg->mad_synth);
	mad_frame_init(&mpeg->mad_frame);
	mpeg->w_pos = mpeg->buf;
	decoder->pcm.data = mpeg->audio_buf;
	decoder->type = DECODER_MPEG;
	ESP_LOGI(TAG, "mad init");
	return ESP_OK;
}


static int32_t decoder_mpeg_scale(mad_fixed_t sample) {
	int32_t output = sample << (sizeof(int32_t) * 8 - MAD_F_FRACBITS - 1);
	if (sample < 0) {
		output |= 1L << (sizeof(int32_t)-1);
	}
	return output;
}


static void decoder_mpeg_prepare_audio(decoder_t *decoder, decoder_pcm_data_t *pcm_data) {
	decoder_data_mpeg_t *mpeg = &decoder->data.mpeg;
	struct mad_pcm *pcm = &mpeg->mad_synth.pcm;
	const mad_fixed_t *right_ch = pcm->channels == 2 ? pcm->samples[1] : pcm->samples[0];
	const mad_fixed_t *left_ch = pcm->samples[0];
	int32_t sample;
	for (size_t i = 0; i < pcm->length * 2; ++i) {
		if (i % 2 == 0) {
			sample = *right_ch++;
		}
		else {
			sample = *left_ch++;
		}
		pcm_data->data[i] = decoder_mpeg_scale(sample);
	}
	pcm_data->length = pcm->length;
}


typedef uint32_t framebuffer_t[256][1008];
extern framebuffer_t *framebuffer;


static void decoder_mpeg_feed(decoder_t *decoder, char *buf, ssize_t size) {
	decoder_data_mpeg_t *mpeg = &decoder->data.mpeg;
	size_t processed_size = mpeg->w_pos - mpeg->buf;
	if (processed_size + MAD_BUFFER_GUARD + size >= MAX_FRAME_SIZE) {
		memmove(mpeg->buf, mpeg->w_pos, MAX_FRAME_SIZE - MAD_BUFFER_GUARD - processed_size);
		mpeg->w_pos = mpeg->buf;
	}
	memcpy(mpeg->w_pos, buf, size);
	mpeg->w_pos += size;
}


static decoder_pcm_data_t *decoder_mpeg_decode(decoder_t *decoder) {
	decoder_data_mpeg_t *mpeg = &decoder->data.mpeg;
	const unsigned char *r_buffer = mpeg->buf;
	decoder->pcm.length = 0;
	decoder_pcm_data_t *out = &decoder->pcm;

	while (1) {
		mad_stream_buffer(&mpeg->mad_stream, r_buffer, mpeg->w_pos - r_buffer);
		if (mad_frame_decode(&mpeg->mad_frame, &mpeg->mad_stream)) {
			if (mpeg->mad_stream.error == MAD_ERROR_BUFLEN) {
				break;
			}
			else if (!MAD_RECOVERABLE(mpeg->mad_stream.error)) {
				ESP_LOGW(TAG, "mad_error %s", mad_stream_errorstr(&mpeg->mad_stream));
				out = NULL;
				break;
			}
		}
		else {
			mad_synth_frame(&mpeg->mad_synth, &mpeg->mad_frame);
			decoder_mpeg_prepare_audio(decoder, &decoder->pcm);
			if (mpeg->mad_stream.next_frame) {
				r_buffer = mpeg->mad_stream.next_frame;
			}
			break;
		}

		if (!mpeg->mad_stream.next_frame) {
			out = NULL;
			break;
		}
		else {
			r_buffer = mpeg->mad_stream.next_frame;
		}
	}

	if (r_buffer != mpeg->buf) {
		size_t processed_size = r_buffer - mpeg->buf;
		memmove(mpeg->buf, r_buffer, MAX_FRAME_SIZE - MAD_BUFFER_GUARD - processed_size);
		mpeg->w_pos -= processed_size;
	}

	return out;
}


static void decoder_mpeg_destroy(decoder_t *decoder) {
	decoder->type = DECODER_UNKNOWN;
	decoder_data_mpeg_t *mpeg = &decoder->data.mpeg;
	mad_synth_finish(&mpeg->mad_synth);
	mad_frame_finish(&mpeg->mad_frame);
	mad_stream_finish(&mpeg->mad_stream);
	ESP_LOGI(TAG, "mad destroy");
}


/* Generic */


esp_err_t decoder_init(decoder_t *decoder, source_t *source) {
	decoder->type = DECODER_UNKNOWN;
	decoder->pcm.data = NULL;
	decoder->pcm.length = 0;
	if (strcmp(source->content_type, "audio/mpeg") == 0) {
		return decoder_mpeg_init(decoder, source);
	}

	ESP_LOGW(TAG, "cannot decode content-type %s", source->content_type);
	return ESP_FAIL;
}


void decoder_feed(decoder_t *decoder, char *buf, ssize_t size) {
	if (decoder->type == DECODER_MPEG) {
		decoder_mpeg_feed(decoder, buf, size);
	}
}


decoder_pcm_data_t *decoder_decode(decoder_t *decoder) {
	if (decoder->type == DECODER_MPEG) {
		return decoder_mpeg_decode(decoder);
	}
	return NULL;
}


void decoder_destroy(decoder_t *decoder) {
	if (decoder->type == DECODER_MPEG) {
		decoder_mpeg_destroy(decoder);
	}
	decoder->pcm.data = NULL;
	decoder->pcm.length = 0;
}
