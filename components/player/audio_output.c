#include "esp_log.h"

#include "audio_output.h"

static const char *TAG = "audio_output";

#ifdef SIMULATOR
static char *device = "default";
#else
static i2s_dev_t* I2S[I2S_NUM_MAX] = {&I2S0, &I2S1};
#endif


esp_err_t audio_output_init(audio_output_t *output) {
#ifdef SIMULATOR
	int err;

	if ((err = snd_pcm_open(&output->handle, device, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK)) < 0) {
		ESP_LOGE(TAG, "Playback open error: %s", snd_strerror(err));
		return ESP_FAIL;
	}

	snd_pcm_hw_params_t *hw_params;

	snd_pcm_hw_params_malloc (&hw_params);
	snd_pcm_hw_params_any(output->handle, hw_params);

	unsigned int rrate = 44100;
	snd_pcm_hw_params_set_access(output->handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(output->handle, hw_params, SND_PCM_FORMAT_S32);
	snd_pcm_hw_params_set_rate_near(output->handle, hw_params, &rrate, NULL);
	snd_pcm_hw_params_set_channels(output->handle, hw_params, 2);
	snd_pcm_uframes_t buffer_size = 48;
	snd_pcm_uframes_t period_size = 576;

	snd_pcm_hw_params_set_buffer_size_near(output->handle, hw_params, &buffer_size);
	snd_pcm_hw_params_set_period_size_near(output->handle, hw_params, &period_size, NULL);
	snd_pcm_hw_params(output->handle, hw_params);
	if ((err = snd_pcm_hw_params(output->handle, hw_params)) < 0) {
		ESP_LOGE(TAG, "HW set params error: %s", snd_strerror(err));
		snd_pcm_close(output->handle);
		snd_pcm_hw_params_free(hw_params);
		output->handle = NULL;
		return ESP_FAIL;
	}
	snd_pcm_hw_params_free(hw_params);

	if ((err = snd_pcm_set_params(output->handle, SND_PCM_FORMAT_S32, SND_PCM_ACCESS_RW_INTERLEAVED, 2, output->sample_rate, 1, 500000)) < 0) {
		ESP_LOGE(TAG, "Playback set params error: %s", snd_strerror(err));
		snd_pcm_close(output->handle);
		output->handle = NULL;
		return ESP_FAIL;
	}

	return ESP_OK;
#else
	static const i2s_config_t i2s_config = {
		.mode = I2S_MODE_MASTER | I2S_MODE_TX,
		.sample_rate = 44100,
		.bits_per_sample = AUDIO_BITS_PER_SAMPLE,
		.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
		.communication_format = I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB,
		.intr_alloc_flags = 0,
		.dma_buf_count = AUDIO_OUTPUT_BUFFER_COUNT,
		.dma_buf_len = AUDIO_OUTPUT_BUFFER_SIZE,
		.use_apll = false,
		.tx_desc_auto_clear = true,
	};
	static const i2s_pin_config_t pin_config = {
		.bck_io_num = GPIO_NUM_26,
		.ws_io_num = GPIO_NUM_25,
		.data_out_num = GPIO_NUM_22,
		.data_in_num = I2S_PIN_NO_CHANGE
	};
	ESP_ERROR_CHECK(i2s_driver_install(output->port, &i2s_config, 0, NULL));
	ESP_ERROR_CHECK(i2s_set_sample_rates(output->port, 44100));
	ESP_ERROR_CHECK(i2s_set_pin(output->port, &pin_config));
	ESP_LOGI(TAG, "I2S initialized");
	return ESP_OK;
#endif
}


esp_err_t audio_output_set_sample_rate(audio_output_t *output, int rate) {
#ifdef SIMULATOR
	int err;
	if ((err = snd_pcm_set_params(output->handle, SND_PCM_FORMAT_S32, SND_PCM_ACCESS_RW_INTERLEAVED, 2, output->sample_rate, 1, 500000)) < 0) {
		ESP_LOGE(TAG, "Playback set params error: %s", snd_strerror(err));
		return ESP_FAIL;
	}

	return ESP_OK;
#else
	int sample_size = AUDIO_BITS_PER_SAMPLE * 2;
	int i2s_frequency = APB_CLK_FREQ / 2;

	// Clock frequency is 1 / (N + (b / a))
	// clock divisor is required denominator value
	double requested_divisor = (double)(i2s_frequency) / (rate * sample_size);
	if (requested_divisor >= 254 || requested_divisor < 2) {
		ESP_LOGE(TAG, "Unsupported sample rate: %d", rate);
		return;
	}

	// Best solution
	double min_error = 1.0;
	int best_n = (int)requested_divisor;
	int best_b = 0;
	int best_a = 1;

	// Bruteforce search solution
	for (int a = 1; a < 64; ++a) {
		int b = (requested_divisor - (double)best_n) * (double)a;
		if (b > 63) {
			continue;
		}

		double divisor = (double)best_n + (double)b / (double)a;
		double error = divisor > requested_divisor ? divisor - requested_divisor : requested_divisor - divisor;
		if (error < min_error) {
			min_error = error;
			best_a = a;
			best_b = b;
		}

		b++;
		if (b > 63) {
			continue;
		}
		divisor = (double)best_n + (double)b / (double)a;
		error = divisor > requested_divisor ? divisor - requested_divisor : requested_divisor - divisor;
		if (error < min_error) {
			min_error = error;
			best_a = a;
			best_b = b;
		}
	}

	// Compare requested rate to final rate
	double final_rate = (double)(i2s_frequency) / (double)sample_size / ((double)best_n + (double)best_b / (double)best_a);
	ESP_LOGI(TAG, "Requested samplerate change to: %d, final samplerate %f, N: %d, b: %d, a: %d", rate, final_rate, best_n, best_b, best_a);

	// Change clock
	I2S[output->port]->clkm_conf.clkm_div_a = 63;
	I2S[output->port]->clkm_conf.clkm_div_b = best_b;
	I2S[output->port]->clkm_conf.clkm_div_a = best_a;
	I2S[output->port]->clkm_conf.clkm_div_num = best_n;

	// Only for information
	output->sample_rate = rate;

	return ESP_OK;
#endif
}


esp_err_t audio_output_write(audio_output_t *output, audio_sample_t *buf, size_t samples_count) {
#ifdef SIMULATOR
	snd_pcm_sframes_t frames;
	snd_pcm_sframes_t written_frames = 0;

	while (written_frames < samples_count) {
		frames = snd_pcm_writei(output->handle, buf + (written_frames * 2), samples_count - written_frames);
		if (frames == -EAGAIN) {
			vTaskDelay(1);
			continue;
		}
		else if (frames >= 0) {
			written_frames += frames;
		}
		else {
			frames = snd_pcm_recover(output->handle, frames, 0);
			if (frames < 0) {
				ESP_LOGE(TAG, "snd_pcm_writei failed: %s", snd_strerror(frames));
				return ESP_FAIL;
			}
		}
	}

	return ESP_OK;
#else
	size_t bytes_written;
	i2s_write(output->port, buf, samples_count * sizeof(audio_sample_t) * 2, &bytes_written, portMAX_DELAY);
	return ESP_OK;
#endif
}


esp_err_t  audio_output_destroy(audio_output_t *output) {
#ifdef SIMULATOR
	snd_pcm_close(output->handle);
	output->handle = NULL;
	return ESP_OK;
#else
	i2s_driver_uninstall(output->port);
	ESP_LOGI(TAG, "I2S stopped");
	return ESP_OK;
#endif
}
