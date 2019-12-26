#pragma once

#include "esp_err.h"

#define AUDIO_OUTPUT_BUFFER_COUNT 4
#define AUDIO_OUTPUT_BUFFER_SIZE 375

#ifdef SIMULATOR

#include <alsa/asoundlib.h>

#else

#include "driver/i2s.h"
#define AUDIO_BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_32BIT
#define AUDIO_I2S_PORT I2S_NUM_0
#define I2S_BASE_CLK (2*APB_CLK_FREQ)

#endif // SIMULATOR


typedef int32_t audio_sample_t;
typedef struct audio_output_t {
	uint32_t sample_rate;
#ifndef SIMULATOR
	i2s_port_t port;
#else
	snd_pcm_t *handle;
	SemaphoreHandle_t write_semaphore;
#endif
} audio_output_t;


esp_err_t audio_output_init(audio_output_t *output);
esp_err_t audio_output_set_sample_rate(audio_output_t *output, int rate);
esp_err_t audio_output_write(audio_output_t *output, audio_sample_t *buf, size_t buf_size);
esp_err_t audio_output_destroy(audio_output_t *output);
