#include "init.h"
#include "player.h"


void app_main(void)
{
	init_nvs();
	init_audio_output();
	init_player();
	init_events();
	init_network();
	init_control();
	for (;;) {
		vTaskDelay(10000);
		printf("tick\n");
	}
}
