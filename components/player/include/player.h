#pragma once

#include "control.h"

#define PLAY_RETRY_TIMEOUT 1000
#define STREAM_BUFFER_SIZE (64 * 1024)
#define AUDIO_PROCESS_BUFFER_SIZE 1024
#define SOURCE_READ_BUFFER_SIZE 1024


void init_player(void);
void player_task(void *arg);
void read_task(void *arg);
void player_stats_task(void *arg);
