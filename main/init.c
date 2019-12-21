#include "freertos/FreeRTOS.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "nvs_flash.h"

#include "events.h"
#include "init.h"


static const char *TAG = "init";
esp_netif_t *network_interface = NULL;


void init_nvs(void) {
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
	ESP_LOGI(TAG, "init_nvs done");
}


void init_wifi_sta(void) {
	ESP_ERROR_CHECK(esp_netif_init());
	
	network_interface = esp_netif_create_default_wifi_sta();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &network_event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &network_event_handler, NULL));

#if CONFIG_NETWORK_STATIC_CONFIGURATION == 1
	ip4_addr_t ip4_ip;
	ip4_addr_t ip4_gw;
	ip4_addr_t ip4_netmask;
	if (!ip4addr_aton(CONFIG_IPV4_ADDRESS, &ip4_ip)) {
		ESP_LOGE(TAG, "IP address in wrong format");
	}
	if (!ip4addr_aton(CONFIG_IPV4_GATEWAY, &ip4_gw)) {
		ESP_LOGE(TAG, "Gateway in wrong format");
	}
	if (!ip4addr_aton(CONFIG_IPV4_MASK, &ip4_netmask)) {
		ESP_LOGE(TAG, "Netmask in wrong format");
	}

	esp_netif_ip_info_t ip_address;
	IP4_ADDR(&ip_address.ip, ip4_addr1_val(ip4_ip), ip4_addr2_val(ip4_ip), ip4_addr3_val(ip4_ip), ip4_addr4_val(ip4_ip));
	IP4_ADDR(&ip_address.gw, ip4_addr1_val(ip4_gw), ip4_addr2_val(ip4_gw), ip4_addr3_val(ip4_gw), ip4_addr4_val(ip4_gw));
	IP4_ADDR(&ip_address.netmask, ip4_addr1_val(ip4_netmask), ip4_addr2_val(ip4_netmask), ip4_addr3_val(ip4_netmask), ip4_addr4_val(ip4_netmask));

	ESP_ERROR_CHECK(esp_netif_dhcps_stop(network_interface));
	ESP_ERROR_CHECK(esp_netif_dhcpc_stop(network_interface));
	ESP_ERROR_CHECK(esp_netif_set_ip_info(network_interface, &ip_address));
#endif

	wifi_config_t wifi_config = {
		.sta = {
			.ssid = CONFIG_WIFI_SSID,
			.password = CONFIG_WIFI_PASSWORD,
		},
	};

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGI(TAG, "init_wifi_sta done");
}


void init_network(void) {
	init_wifi_sta();
}
