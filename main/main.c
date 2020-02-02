#include "control_http.h"
#include "init.h"
#include "player.h"
#include "sdkconfig.h"


void app_main(void)
{
	init_nvs();
	init_event_loop();
	init_player();
	init_tasks();
	init_network();
	init_framebuffer();
	for (;;) {
		vTaskDelay(10000);
		printf("tick\n");
	}
}
