#include "init.h"


void app_main(void)
{
	init_nvs();
	init_events();
	init_network();
}
