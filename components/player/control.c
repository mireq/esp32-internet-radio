#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

#include "control.h"


#if CONFIG_UDP_CONTROL_PORT
static const char *TAG = "control";
#endif


#if CONFIG_UDP_CONTROL_PORT
void udp_control_loop(void *arg) {
	static char rx_buffer[256];
	struct sockaddr_in dest_addr;
	int addr_family;
	int ip_protocol;

	dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons(CONFIG_UDP_CONTROL_PORT);
	addr_family = AF_INET;
	ip_protocol = IPPROTO_IP;

	int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
	if (sock < 0) {
		ESP_LOGE(TAG, "UDP socket not created");
		vTaskDelete(NULL);
		return;
	}

	int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
	if (err < 0) {
		ESP_LOGE(TAG, "UDP socket not bound");
		vTaskDelete(NULL);
		return;
	}

	ESP_LOGI(TAG, "enabled udp control");

	while (1) {
		struct sockaddr_in6 source_addr;
		socklen_t socklen = sizeof(source_addr);
		int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
		if (len < 0) {
			if (errno == EINTR) {
				continue;
			}
			ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
			break;
		}
		else {
			rx_buffer[len] = 0;
			ESP_LOGI(TAG, "%s\n", rx_buffer);
		}
		vTaskDelay(1);
	}

	vTaskDelete(NULL);
}
#endif


void init_control(void) {
#if CONFIG_UDP_CONTROL_PORT
	if (xTaskCreatePinnedToCore(&udp_control_loop, "udp_control", 4096, NULL, 5, NULL, 0) != pdPASS) {
		ESP_LOGE(TAG, "UDP control task not initialized");
	}
#endif
}
