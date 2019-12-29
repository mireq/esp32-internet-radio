#pragma once


#define PLAY_RETRY_TIMEOUT 1000
#define STREAM_BUFFER_SIZE (64 * 1024)
#define AUDIO_PROCESS_BUFFER_SIZE 1024


void init_player_events(void);
void start_playback(void);
void stop_playback(void);
void init_player(void);
void init_audio_output(void);
