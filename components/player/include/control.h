#pragma once


typedef enum control_command_t {
	CONTROL_COMMAND_STOP,
	CONTROL_COMMAND_START,
	CONTROL_COMMAND_VOLUME_UP,
	CONTROL_COMMAND_VOLUME_DOWN,
	CONTROL_COMMAND_VOLUME_SET,
	CONTROL_COMMAND_STREAM_URL,
} control_command_t;


void init_control(void);
