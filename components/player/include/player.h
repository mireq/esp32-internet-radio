#pragma once

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"


#define PLAY_RETRY_TIMEOUT 1000
#define STREAM_BUFFER_SIZE (1024 * 1024)
#define AUDIO_PROCESS_BUFFER_SIZE 1024
#define SOURCE_READ_BUFFER_SIZE 1024


typedef struct player_state_t {
	SemaphoreHandle_t access_mutex;
	uint16_t volume;
} player_state_t;

extern player_state_t player_state;


void init_player(void);
void player_task(void *arg);
void read_task(void *arg);
void player_stats_task(void *arg);
