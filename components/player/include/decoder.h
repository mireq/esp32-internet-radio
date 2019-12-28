#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "mad.h"

#include "source.h"


struct decoder_t;

typedef enum decoder_event_type_t {
	DECODER_PCM,
	DECODER_STATUS,
} decoder_event_type_t;


typedef void (*decoder_callback_t) (struct decoder_t*, decoder_event_type_t, void *data);


typedef enum decoder_type_t {
	DECODER_UNKNOWN,
	DECODER_MPEG,
} decoder_type_t;


typedef struct decoder_data_mpeg_t {
	struct mad_stream mad_stream;
	struct mad_frame mad_frame;
	struct mad_synth mad_synth;
} decoder_data_mpeg_t;


typedef struct decoder_t {
	decoder_type_t type;
	decoder_callback_t callback;
	union {
		decoder_data_mpeg_t mpeg;
	} data;
} decoder_t;


esp_err_t decoder_init(decoder_t *decoder, source_t *source);
esp_err_t decoder_feed(decoder_t *decoder, char *buf, ssize_t size);
void decoder_destroy(decoder_t *decoder);
