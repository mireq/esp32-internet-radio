#pragma once

#include "esp_event.h"
#include "playlist.h"
#include "player.h"


ESP_EVENT_DECLARE_BASE(NETWORK_EVENT);
ESP_EVENT_DECLARE_BASE(PLAYBACK_EVENT);
ESP_EVENT_DECLARE_BASE(CONTROL_COMMAND);


typedef enum {
	NETWORK_EVENT_DISCONNECT,
	NETWORK_EVENT_CONNECT,
} network_event_t;


typedef enum {
	PLAYBACK_EVENT_START,
	PLAYBACK_EVENT_FINISH,
	PLAYBACK_EVENT_ERROR,
	PLAYBACK_EVENT_PLAYLIST_ITEM_CHANGE,
} playback_event_t;


typedef enum {
	CONTROL_COMMAND_SET_VOLUME,
} control_command_t;


extern esp_event_loop_handle_t player_event_loop;
extern playlist_t playlist;
extern player_state_t player_state;
