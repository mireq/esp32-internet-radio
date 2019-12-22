#include "init.h"
#include "player.h"


void app_main(void)
{
	init_nvs();
	init_player();
	init_events();
	init_network();
	for (;;) {
		vTaskDelay(1000);
		printf("tick\n");
	}
}
