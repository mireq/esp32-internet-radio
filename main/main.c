#include "control_http.h"
#include "init.h"
#include "player.h"
#include "sdkconfig.h"


void app_main(void)
{
	init_nvs();
	init_audio_output();
	init_player();
	init_events();
	init_network();
#if CONFIG_HTTP_CONTROL
	init_http_control();
#endif
	init_framebuffer();
	for (;;) {
		vTaskDelay(10000);
		printf("tick\n");
	}
}
