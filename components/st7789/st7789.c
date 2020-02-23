#include <stdbool.h>
#include <string.h>

#include "st7789.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"


const char *TAG = "st7789";


static void st7789_pre_cb(spi_transaction_t *transaction) {
	const st7789_transaction_data_t *data = (st7789_transaction_data_t *)transaction->user;
	gpio_set_level(data->driver->pin_dc, data->data);
}


esp_err_t st7789_init(st7789_driver_t *driver) {
	driver->buffer = (st7789_color_t *)heap_caps_malloc(driver->buffer_size * 2 * sizeof(st7789_color_t), MALLOC_CAP_DMA);
	if (driver->buffer == NULL) {
		ESP_LOGE(TAG, "buffer not allocated");
		return ESP_FAIL;
	}
	driver->buffer_a = driver->buffer;
	driver->buffer_b = driver->buffer + driver->buffer_size;
	driver->current_buffer = driver->buffer_a;
	driver->queue_fill = 0;

	driver->data.driver = driver;
	driver->data.data = true;
	driver->command.driver = driver;
	driver->command.data = false;

	gpio_pad_select_gpio(driver->pin_reset);
	gpio_pad_select_gpio(driver->pin_dc);
	gpio_set_direction(driver->pin_reset, GPIO_MODE_OUTPUT);
	gpio_set_direction(driver->pin_dc, GPIO_MODE_OUTPUT);

	spi_bus_config_t buscfg = {
		.mosi_io_num=driver->pin_mosi,
		.miso_io_num=-1,
		.sclk_io_num=driver->pin_sclk,
		.quadwp_io_num=-1,
		.quadhd_io_num=-1,
		.max_transfer_sz=driver->buffer_size * 2 * sizeof(st7789_color_t), // 2 buffers with 2 bytes for pixel
		.flags=SPICOMMON_BUSFLAG_NATIVE_PINS
	};
	spi_device_interface_config_t devcfg = {
		.clock_speed_hz=SPI_MASTER_FREQ_40M,
		.mode=3,
		.spics_io_num=-1,
		.queue_size=ST7789_SPI_QUEUE_SIZE,
		.pre_cb=st7789_pre_cb,
	};

	if (spi_bus_initialize(driver->spi_host, &buscfg, driver->dma_chan) != ESP_OK) {
		ESP_LOGE(TAG, "spi bus initialize failed");
		return ESP_FAIL;
	}
	if (spi_bus_add_device(driver->spi_host, &devcfg, &driver->spi) != ESP_OK) {
		ESP_LOGE(TAG, "spi bus add device failed");
		return ESP_FAIL;
	}
	ESP_LOGI(TAG, "driver initialized");
	return ESP_OK;
}


void st7789_reset(st7789_driver_t *driver) {
	gpio_set_level(driver->pin_reset, 0);
	vTaskDelay(10 / portTICK_PERIOD_MS);
	gpio_set_level(driver->pin_reset, 1);
	vTaskDelay(120 / portTICK_PERIOD_MS);
}


void st7789_lcd_init(st7789_driver_t *driver) {
	const uint8_t caset[4] = {
		0x00,
		0x00,
		(driver->display_width - 1) >> 8,
		(driver->display_width - 1) & 0xff
	};
	const uint8_t raset[4] = {
		0x00,
		0x00,
		(driver->display_height - 1) >> 8,
		(driver->display_height - 1) & 0xff
	};
	const st7789_command_t init_sequence[] = {
		// Sleep
		{ST7789_CMD_SLPIN, 10, 0, NULL},                    // Sleep
		{ST7789_CMD_SWRESET, 200, 0, NULL},                 // Reset
		{ST7789_CMD_SLPOUT, 120, 0, NULL},                  // Sleep out

		{ST7789_CMD_MADCTL, 0, 1, (const uint8_t *)"\x00"}, // Page / column address order
		{ST7789_CMD_COLMOD, 0, 1, (const uint8_t *)"\x55"}, // 16 bit RGB
		{ST7789_CMD_INVON, 0, 0, NULL},                     // Inversion on
		{ST7789_CMD_CASET, 0, 4, (const uint8_t *)&caset},  // Set width
		{ST7789_CMD_RASET, 0, 4, (const uint8_t *)&raset},  // Set height
		// Porch setting
		//{ST7789_CMD_PORCTRL, 0, 5, (const uint8_t *)"\x0c\x0c\x00\x33\x33"},
		// Set VGH to 13.26V and VGL to -10.43V
		{ST7789_CMD_GCTRL, 0, 1, (const uint8_t *)"\x35"},
		// Set VCOM to 1.675V
		{ST7789_CMD_VCOMS, 0, 1, (const uint8_t *)"\x1f"},
		// LCM control
		{ST7789_CMD_LCMCTRL, 0, 1, (const uint8_t *)"\x2c"},
		// VDV/VRH command enable
		{ST7789_CMD_VDVVRHEN, 0, 2, (const uint8_t *)"\x01\xc3"},
		// VDV set to default value
		{ST7789_CMD_VDVSET, 0, 1, (const uint8_t *)"\x20"},
		 // Set frame rate to 111Hz
		{ST7789_CMD_FRCTR2, 0, 1, (const uint8_t *)"\x01"},
		// Set VDS to 2.3V, AVCL to -4.8V and AVDD to 6.8V
		{ST7789_CMD_PWCTRL1, 0, 2, (const uint8_t *)"\xa4\xa1"},
		// Gamma corection
		//{ST7789_CMD_GAMSET, 0, 1, (const uint8_t *)"\x01"},
		{ST7789_CMD_PVGAMCTRL, 0, 14, (const uint8_t *)"\xd0\x08\x11\x08\x0c\x15\x39\x33\x50\x36\x13\x14\x29\x2d"},
		{ST7789_CMD_NVGAMCTRL, 0, 14, (const uint8_t *)"\xd0\x08\x10\x08\x06\x06\x39\x44\x51\x0b\x16\x14\x2f\x31"},
		// Little endian
		{ST7789_CMD_RAMCTRL, 0, 2, (const uint8_t *)"\x00\xc8"},
		{ST7789_CMDLIST_END, 0, 0, NULL},                   // End of commands
	};
	st7789_run_commands(driver, init_sequence);
	st7789_clear(driver, 0x0000);
	const st7789_command_t init_sequence2[] = {
		{ST7789_CMD_DISPON, 100, 0, NULL},                  // Display on
		{ST7789_CMD_SLPOUT, 100, 0, NULL},                  // Sleep out
		{ST7789_CMD_TEOFF, 0, 0, NULL},                      // Tearing line effect on
		{ST7789_CMDLIST_END, 0, 0, NULL},                   // End of commands
	};
	st7789_run_commands(driver, init_sequence2);
}


void st7789_start_command(st7789_driver_t *driver) {
	gpio_set_level(driver->pin_dc, 0);
}


void st7789_start_data(st7789_driver_t *driver) {
	gpio_set_level(driver->pin_dc, 1);
}

void st7789_run_command(st7789_driver_t *driver, const st7789_command_t *command) {
	spi_transaction_t *rtrans;
	st7789_wait_until_queue_empty(driver);
	spi_transaction_t trans;
	memset(&trans, 0, sizeof(trans));
	trans.length = 8; // 8 bits
	trans.tx_buffer = &command->command;
	trans.user = &driver->command;
	spi_device_queue_trans(driver->spi, &trans, portMAX_DELAY);
	spi_device_get_trans_result(driver->spi, &rtrans, portMAX_DELAY);

	if (command->data_size > 0) {
		memset(&trans, 0, sizeof(trans));
		trans.length = command->data_size * 8;
		trans.tx_buffer = command->data;
		trans.user = &driver->data;
		spi_device_queue_trans(driver->spi, &trans, portMAX_DELAY);
		spi_device_get_trans_result(driver->spi, &rtrans, portMAX_DELAY);
	}

	if (command->wait_ms > 0) {
		vTaskDelay(command->wait_ms / portTICK_PERIOD_MS);
	}
}

void st7789_run_commands(st7789_driver_t *driver, const st7789_command_t *sequence) {
	while (sequence->command != ST7789_CMDLIST_END) {
		st7789_run_command(driver, sequence);
		sequence++;
	}
}

void st7789_clear(st7789_driver_t *driver, st7789_color_t color) {
	st7789_fill_area(driver, color, 0, 0, driver->display_width, driver->display_height);
}

void st7789_fill_area(st7789_driver_t *driver, st7789_color_t color, uint16_t start_x, uint16_t start_y, uint16_t width, uint16_t height) {
	for (size_t i = 0; i < driver->buffer_size * 2; ++i) {
		driver->buffer[i] = color;
	}
	st7789_set_window(driver, start_x, start_y, start_x + width - 1, start_y + height - 1);

	size_t bytes_to_write = width * height * 2;
	size_t transfer_size = driver->buffer_size * 2 * sizeof(st7789_color_t);

	spi_transaction_t trans;
	memset(&trans, 0, sizeof(trans));
	trans.tx_buffer = driver->buffer;
	trans.user = &driver->data;
	trans.length = transfer_size * 8;
	trans.rxlength = 0;

	spi_transaction_t *rtrans;

	while (bytes_to_write > 0) {
		if (driver->queue_fill >= ST7789_SPI_QUEUE_SIZE) {
			spi_device_get_trans_result(driver->spi, &rtrans, portMAX_DELAY);
			driver->queue_fill--;
		}
		if (bytes_to_write < transfer_size) {
			transfer_size = bytes_to_write;
		}
		spi_device_queue_trans(driver->spi, &trans, portMAX_DELAY);
		driver->queue_fill++;
		bytes_to_write -= transfer_size;
	}

	st7789_wait_until_queue_empty(driver);
}

void st7789_set_window(st7789_driver_t *driver, uint16_t start_x, uint16_t start_y, uint16_t end_x, uint16_t end_y) {
	uint8_t caset[4];
	uint8_t raset[4];
	caset[0] = (uint8_t)(start_x >> 8);
	caset[1] = (uint8_t)(start_x & 0xff);
	caset[2] = (uint8_t)(end_x >> 8);
	caset[3] = (uint8_t)(end_x & 0xff);
	raset[0] = (uint8_t)(start_y >> 8);
	raset[1] = (uint8_t)(start_y & 0xff);
	raset[2] = (uint8_t)(end_y >> 8);
	raset[3] = (uint8_t)(end_y & 0xff);
	st7789_command_t sequence[] = {
		{ST7789_CMD_CASET, 0, 4, caset},
		{ST7789_CMD_RASET, 0, 4, raset},
		{ST7789_CMD_RAMWR, 0, 0, NULL},
		{ST7789_CMDLIST_END, 0, 0, NULL},
	};
	st7789_run_commands(driver, sequence);
}

void st7789_write_pixels(st7789_driver_t *driver, st7789_color_t *pixels, size_t length) {
	st7789_wait_until_queue_empty(driver);

	spi_transaction_t *trans = driver->current_buffer == driver->buffer_a ? &driver->trans_a : &driver->trans_b;
	memset(trans, 0, sizeof(&trans));
	trans->tx_buffer = driver->current_buffer;
	trans->user = &driver->data;
	trans->length = length * sizeof(st7789_color_t) * 8;
	trans->rxlength = 0;

	spi_device_queue_trans(driver->spi, trans, portMAX_DELAY);
	driver->queue_fill++;
}

void st7789_wait_until_queue_empty(st7789_driver_t *driver) {
	spi_transaction_t *rtrans;
	while (driver->queue_fill > 0) {
		spi_device_get_trans_result(driver->spi, &rtrans, portMAX_DELAY);
		driver->queue_fill--;
	}
}

void st7789_swap_buffers(st7789_driver_t *driver) {
	st7789_write_pixels(driver, driver->current_buffer, driver->buffer_size);
	driver->current_buffer = driver->current_buffer == driver->buffer_a ? driver->buffer_b : driver->buffer_a;
}

st7789_color_t st7789_rgb_to_color(uint8_t r, uint8_t g, uint8_t b) {
	return (((uint16_t)r >> 3) << 11) | (((uint16_t)g >> 2) << 5) | ((uint16_t)b >> 3);
}
