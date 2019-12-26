#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

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

	unsigned int rrate = 48000;
	snd_pcm_hw_params_set_access (output->handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(output->handle, hw_params, SND_PCM_FORMAT_S32_LE);
	snd_pcm_hw_params_set_rate_near(output->handle, hw_params, &rrate, NULL);
	snd_pcm_hw_params_set_channels(output->handle, hw_params, 2);
	snd_pcm_uframes_t buffer_size = 8192;
	snd_pcm_uframes_t period_size = 1;

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

	if ((err = snd_pcm_set_params(output->handle, SND_PCM_FORMAT_S32_LE, SND_PCM_ACCESS_RW_INTERLEAVED, 2, output->sample_rate, 1, 500000)) < 0) {
		ESP_LOGE(TAG, "Playback set params error: %s", snd_strerror(err));
		snd_pcm_close(output->handle);
		output->handle = NULL;
		return ESP_FAIL;
	}

	return ESP_OK;
#else
	i2s_config_t i2s_config = {
		.mode = I2S_MODE_MASTER | I2S_MODE_TX,
		.sample_rate = output->sample_rate,
		.bits_per_sample = AUDIO_BITS_PER_SAMPLE,
		.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
		.communication_format = I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB,
		.intr_alloc_flags = ESP_INTR_FLAG_LEVEL2,
		.dma_buf_count = AUDIO_OUTPUT_BUFFER_COUNT,
		.dma_buf_len = AUDIO_OUTPUT_BUFFER_SIZE,
		.use_apll = false,
	};
	i2s_pin_config_t pin_config = {
		.bck_io_num = GPIO_NUM_26,
		.ws_io_num = GPIO_NUM_25,
		.data_out_num = GPIO_NUM_22,
		.data_in_num = I2S_PIN_NO_CHANGE
	};
	ESP_ERROR_CHECK(i2s_driver_install(output->port, &i2s_config, 0, NULL));
	ESP_ERROR_CHECK(i2s_zero_dma_buffer(output->port));
	ESP_ERROR_CHECK(i2s_set_pin(output->port, &pin_config));
	ESP_LOGI(TAG, "I2S initialized");
	return ESP_OK;
#endif
}


esp_err_t audio_output_set_sample_rate(audio_output_t *output, int rate) {
#ifdef SIMULATOR
	int err;
	if ((err = snd_pcm_set_params(output->handle, SND_PCM_FORMAT_S32_BE, SND_PCM_ACCESS_RW_INTERLEAVED, 2, output->sample_rate, 1, 500000)) < 0) {
		ESP_LOGE(TAG, "Playback set params error: %s", snd_strerror(err));
		return ESP_FAIL;
	}

	return ESP_OK;
#else
	int factor = (256 % AUDIO_BITS_PER_SAMPLE)? 384 : 256;
	int div_b, bck = 0;
	double denom = (double)1 / 64;
	double clkmdiv = (double)I2S_BASE_CLK / (rate * factor);
	div_b = (clkmdiv - (int)clkmdiv) / denom;
	bck = factor/(I2S_BITS_PER_SAMPLE_32BIT * 2);

	I2S[output->port]->clkm_conf.clkm_div_a = 63;
	I2S[output->port]->clkm_conf.clkm_div_b = div_b;
	I2S[output->port]->clkm_conf.clkm_div_num = (int)clkmdiv;
	I2S[output->port]->sample_rate_conf.tx_bck_div_num = bck;
	I2S[output->port]->sample_rate_conf.rx_bck_div_num = bck;
	output->sample_rate = rate;
	return ESP_OK;
#endif
}


esp_err_t audio_output_write(audio_output_t *output, audio_sample_t *buf, size_t samples_count) {
#ifdef SIMULATOR
	snd_pcm_sframes_t frames;
	frames = snd_pcm_writei(output->handle, buf, samples_count);
	while (frames == -EAGAIN) {
		vTaskDelay(1);
		frames = snd_pcm_writei(output->handle, buf, samples_count);
	}
	if (frames < 0) {
		frames = snd_pcm_recover(output->handle, frames, 0);
	}
	if (frames < 0) {
		ESP_LOGE(TAG, "snd_pcm_writei failed: %s", snd_strerror(frames));
	}

	return ESP_OK;
#else
	size_t bytes_written;
	i2s_write(output->port, buf, samples_count * AUDIO_BITS_PER_SAMPLE * 2, &bytes_written, portMAX_DELAY);
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
