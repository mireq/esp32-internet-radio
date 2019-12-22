#include "freertos/FreeRTOS.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#ifndef SIMULATOR
#include "nvs_flash.h"
#endif

#include "events.h"
#include "init.h"
#include "player.h"


static const char *TAG = "init";


#ifndef SIMULATOR
static const char *NET = "network";
extern esp_netif_t *network_interface;
esp_netif_t *network_interface = NULL;
#endif


#ifndef SIMULATOR


static void network_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		esp_wifi_connect();
	}
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		// retry
		esp_wifi_connect();
		ESP_LOGW(NET, "not connected to AP, retry.");
		esp_event_post_to(player_event_loop, NETWORK_EVENT, NETWORK_EVENT_DISCONNECT, NULL, 0, portMAX_DELAY);
	}
	else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
		ESP_LOGI(NET, "connected with ip address:" IPSTR, IP2STR(&event->ip_info.ip));
#if CONFIG_NETWORK_STATIC_CONFIGURATION == 1
		bool static_configuration = true;
		ip4_addr_t ip4_main_dns;
		ip4_addr_t ip4_backup_dns;
		if (!ip4addr_aton(CONFIG_IPV4_PRIMARY_DNS, &ip4_main_dns)) {
			ESP_LOGE(TAG, "Main DNS in wrong format");
			static_configuration = false;
		}
		if (!ip4addr_aton(CONFIG_IPV4_SECONDARY_DNS, &ip4_backup_dns)) {
			ESP_LOGE(TAG, "Backup DNS in wrong format");
			static_configuration = false;
		}
		esp_netif_dns_info_t main_dns = {
			.ip = {
				.type = ESP_IPADDR_TYPE_V4,
			}
		};
		esp_netif_dns_info_t backup_dns = {
			.ip = {
				.type = ESP_IPADDR_TYPE_V4,
			}
		};
		IP4_ADDR(&main_dns.ip.u_addr.ip4, ip4_addr1_val(ip4_main_dns), ip4_addr2_val(ip4_main_dns), ip4_addr3_val(ip4_main_dns), ip4_addr4_val(ip4_main_dns));
		IP4_ADDR(&backup_dns.ip.u_addr.ip4, ip4_addr1_val(ip4_backup_dns), ip4_addr2_val(ip4_backup_dns), ip4_addr3_val(ip4_backup_dns), ip4_addr4_val(ip4_backup_dns));
		if (static_configuration) {
			ESP_ERROR_CHECK(esp_netif_set_dns_info(network_interface, ESP_NETIF_DNS_MAIN, &main_dns));
			ESP_ERROR_CHECK(esp_netif_set_dns_info(network_interface, ESP_NETIF_DNS_BACKUP, &backup_dns));
		}
#endif
		esp_event_post_to(player_event_loop, NETWORK_EVENT, NETWORK_EVENT_CONNECT, NULL, 0, portMAX_DELAY);
	}
}


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

#if CONFIG_NETWORK_STATIC_CONFIGURATION == 1
	bool static_configuration = true;
	ip4_addr_t ip4_ip;
	ip4_addr_t ip4_gw;
	ip4_addr_t ip4_netmask;
	if (!ip4addr_aton(CONFIG_IPV4_ADDRESS, &ip4_ip)) {
		ESP_LOGE(TAG, "IP address in wrong format");
		static_configuration = false;
	}
	if (!ip4addr_aton(CONFIG_IPV4_GATEWAY, &ip4_gw)) {
		ESP_LOGE(TAG, "Gateway in wrong format");
		static_configuration = false;
	}
	if (!ip4addr_aton(CONFIG_IPV4_MASK, &ip4_netmask)) {
		ESP_LOGE(TAG, "Netmask in wrong format");
		static_configuration = false;
	}

	esp_netif_ip_info_t ip_address;
	IP4_ADDR(&ip_address.ip, ip4_addr1_val(ip4_ip), ip4_addr2_val(ip4_ip), ip4_addr3_val(ip4_ip), ip4_addr4_val(ip4_ip));
	IP4_ADDR(&ip_address.gw, ip4_addr1_val(ip4_gw), ip4_addr2_val(ip4_gw), ip4_addr3_val(ip4_gw), ip4_addr4_val(ip4_gw));
	IP4_ADDR(&ip_address.netmask, ip4_addr1_val(ip4_netmask), ip4_addr2_val(ip4_netmask), ip4_addr3_val(ip4_netmask), ip4_addr4_val(ip4_netmask));

	if (static_configuration) {
		ESP_ERROR_CHECK(esp_netif_dhcps_stop(network_interface));
		ESP_ERROR_CHECK(esp_netif_dhcpc_stop(network_interface));
		ESP_ERROR_CHECK(esp_netif_set_ip_info(network_interface, &ip_address));
	}
#endif

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	wifi_config_t wifi_config = {
		.sta = {
			.ssid = CONFIG_WIFI_SSID,
			.password = CONFIG_WIFI_PASSWORD,
		},
	};

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));

	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &network_event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &network_event_handler, NULL));

	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGI(TAG, "init_wifi_sta done");
}


void init_network(void) {
	init_wifi_sta();
}

#endif


void init_events(void) {
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_event_loop_args_t loop_args = {
		.queue_size = 4,
		.task_name = "player_event",
		.task_priority = 5,
		.task_stack_size = 8192,
		.task_core_id = 0,
	};
	ESP_ERROR_CHECK(esp_event_loop_create(&loop_args, &player_event_loop));
	init_player_events();
}
