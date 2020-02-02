#include <stdbool.h>
#include <stdio.h>
#include <strings.h>
#include <sys/param.h>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "mbedtls/base64.h"
#include "mbedtls/sha1.h"

#include "bits.h"
#include "control_http.h"
#include "http_header_parser/http_header_parser.h"
#include "http_server.h"
#include "interface.h"


#if CONFIG_HTTP_CONTROL


static const char *TAG = "control_http";


typedef struct websocket_header_t {
	uint64_t size;
	uint64_t pos;
	char mask[4];
} websocket_header_t;


typedef uint16_t command_code_t;


typedef struct response_message_t {
	command_code_t command;
	uint64_t size;
	char *data;
} response_message_t;


typedef struct http_request_t {
	char query[HTTP_SERVER_QUERY_SIZE];
	char method[5];
	char upgrade[20];
	char host[32];
	char sec_websocket_key[32];
	http_server_t *server;
	int client_socket;
	SemaphoreHandle_t write_data_semaphore;
} http_request_t;



static ssize_t recv_frame_data(int socket, websocket_header_t *header, void *buf, size_t size) {
	if (size > header->size - header->pos) {
		ssize_t received = recv(socket, buf, header->size - header->pos, MSG_WAITALL);
		if (received >= 0) {
			header->pos += received;
		}
		return -1;
	}

	ssize_t received = recv(socket, buf, size, MSG_WAITALL);
	if (received >= 0) {
		uint8_t *buf_data = (uint8_t *)buf;
		for (size_t i = 0; i < received; ++i) {
			buf_data[i] ^= header->mask[(header->pos + i) & 0x03];
		}
		header->pos += received;
	}
	return received;
}


static const char http_not_found[] = "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n";
static const char http_found[] = "HTTP/1.1 302 Found\r\nLocation: %s?device=%s\r\nConnection: close\r\n\r\n";
static const char http_ws_start[] = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ";
static const char ws_magic_guid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

static http_request_t request;
static SemaphoreHandle_t player_status_semaphore;


static void ws_handshake(http_request_t *request) {
	char key[64];
	strcpy(key, request->sec_websocket_key);
	strcpy(key + strlen(key), ws_magic_guid);

	unsigned char sha1sum[20];
	mbedtls_sha1((unsigned char *) key, strlen(key), sha1sum);
	size_t olen;
	mbedtls_base64_encode((unsigned char *)key, sizeof(key), &olen, sha1sum, 20);
	key[olen] = '\0';

	write(request->client_socket, http_ws_start, sizeof(http_ws_start) - 1);
	write(request->client_socket, key, strlen(key));
	write(request->client_socket, "\r\n\r\n", sizeof("\r\n\r\n") - 1);
}


static void default_http_handler(http_request_t *request) {
	write(request->client_socket, http_not_found, sizeof(http_not_found));
}


static void home_http_handler(http_request_t *request) {
	char response[sizeof(http_found) + sizeof(CONFIG_HTTP_WEBINTERFACE_URL) + sizeof(request->host) + 1];
	bzero(response, sizeof(response));
	sprintf(response, http_found, CONFIG_HTTP_WEBINTERFACE_URL, request->host);
	write(request->client_socket, response, strlen(response));
}


static esp_err_t receive_websocket_header(http_request_t *request, websocket_header_t *header) {
	uint8_t header_data[2];
	ssize_t received;
	header->pos = 0;
	received = recv(request->client_socket, header_data, sizeof(header_data), 0);
	if (received == -1) {
		return ESP_FAIL;
	}
	bool has_mask = false;
	if (header_data[0] & 0x80) {
		has_mask = true;
	}
	uint8_t opcode = header_data[0] & 0x0f;
	if (opcode == 0x08) { // Final frame
		return ESP_FAIL;
	}

	// Set header
	header->size = header_data[1] & 0x7f;

	if (header->size == 126) {
		uint16_t size;
		received = recv(request->client_socket, &size, sizeof(size), 0);
		if (received == -1) {
			return ESP_FAIL;
		}
		header->size = ntohs(size);
	}
	else if (header->size == 127) {
		uint64_t size;
		received = recv(request->client_socket, &size, sizeof(size), 0);
		if (received == -1) {
			return ESP_FAIL;
		}
		header->size = ntoh64(size);
	}

	// Read optional mask
	if (has_mask) {
		received = recv(request->client_socket, &header->mask, sizeof(header->mask), MSG_WAITALL);
		if (received == -1) {
			return ESP_FAIL;
		}
	}

	return ESP_OK;
}


static esp_err_t send_websocket_header(http_request_t *request, size_t size) {
	uint8_t header[10];
	size_t header_size;
	header[0] = 0x80 | 0x02; // binary
	if (size < 126) {
		header[1] = size & 0x7f;
		header_size = 2;
	}
	else if (size <= 65535) {
		header[1] = 126;
		header_size = 4;
		uint16_t native_size = htons(size);
		memcpy(&header[2], &native_size, sizeof(native_size));
	}
	else {
		header_size = 10;
		uint64_t native_size = hton64(size);
		memcpy(&header[2], &native_size, sizeof(native_size));
	}
	ssize_t processed;
	processed = send(request->client_socket, &header, header_size, 0);
	if (processed < 0) {
		return ESP_FAIL;
	}
	return ESP_OK;
}


static esp_err_t send_websocket_message(http_request_t *request, response_message_t *message) {
	if (send_websocket_header(request, sizeof(message->command)) != ESP_OK) {
		return ESP_FAIL;
	}

	// To network byte order
	command_code_t command = htons(message->command);

	ssize_t processed;
	processed = send(request->client_socket, &command, sizeof(command), 0);
	if (processed < 0) {
		return ESP_FAIL;
	}

	if (message->size > 0 && message->data != NULL) {
		processed = send(request->client_socket, message->data, message->size, 0);
		if (processed < 0) {
			return ESP_FAIL;
		}
	}
	return ESP_OK;
}


static esp_err_t handle_ping(http_request_t *request, websocket_header_t *header) {
	response_message_t msg = {
		.command = WS_RESPONSE_PONG,
		.size = 0,
		.data = NULL,
	};
	return send_websocket_message(request, &msg);
}


static esp_err_t handle_set_playlist_item(http_request_t *request, websocket_header_t *header) {
	char data[sizeof(((playlist_item_t *)0)->name) + sizeof(((playlist_item_t *)0)->uri)];
	if (header->size > sizeof(data)) {
		return ESP_FAIL;
	}
	ssize_t received = recv_frame_data(request->client_socket, header, &data, header->size - sizeof(command_code_t));
	if (received < 0) {
		return ESP_FAIL;
	}

	data[sizeof(data) - 1] = 0;

	const char *uri = &data[0];
	const char *name = &data[0];

	while (*name) {
		name++;
	}
	name++;

	if (name - uri >= sizeof(data)) {
		return ESP_FAIL;
	}

	playlist_set_item_simple(&playlist, name, uri);
	return ESP_OK;
}


static esp_err_t handle_set_volume(http_request_t *request, websocket_header_t *header) {
	if (header->size != sizeof(command_code_t) + sizeof(uint16_t)) {
		return ESP_FAIL;
	}
	uint16_t volume;
	ssize_t received = recv_frame_data(request->client_socket, header, &volume, sizeof(volume));
	if (received < 0) {
		return ESP_FAIL;
	}
	volume = ntohs(volume);
	esp_event_post_to(player_event_loop, CONTROL_COMMAND, CONTROL_COMMAND_SET_VOLUME, &volume, sizeof(volume), portMAX_DELAY);
	return ESP_OK;
}


static esp_err_t handle_websocket_frame(http_request_t *request, websocket_header_t *header) {
	command_code_t command;

	// Read command
	ssize_t received;
	received = recv_frame_data(request->client_socket, header, &command, sizeof(command));
	if (received < 0) {
		return ESP_FAIL;
	}

	// Network byte order
	command = ntohs(command);

	esp_err_t status = ESP_OK;
	xSemaphoreTake(request->write_data_semaphore, 1000 / portTICK_PERIOD_MS);
	// Handle command
	switch (command) {
		case WS_COMMAND_PING:
			status = handle_ping(request, header);
			break;
		case WS_COMMAND_SET_PLAYLIST_ITEM:
			status = handle_set_playlist_item(request, header);
			break;
		case WS_COMMAND_SET_VOLUME:
			status = handle_set_volume(request, header);
			break;
		default:
			status = ESP_FAIL;
			break;
	}
	xSemaphoreGive(request->write_data_semaphore);
	return status;
}


static esp_err_t send_player_status(http_request_t *request) {
	response_message_t msg = {
		.command = WS_RESPONSE_STATUS,
		.size = 0,
		.data = NULL,
	};
	return send_websocket_message(request, &msg);
}


static void control_http_handler(http_request_t *request) {
	ws_handshake(request);
	while (1) {
		websocket_header_t websocket_header;
		if (receive_websocket_header(request, &websocket_header) != ESP_OK) {
			break;
		}
		if (handle_websocket_frame(request, &websocket_header) != ESP_OK) {
			break;
		}
	}
}


static void on_http_request(http_request_t *request) {
	if (strcmp(request->method, "GET") == 0) {
		if (strcmp(request->query, "/") == 0 && strcmp(request->upgrade, "websocket") == 0) {
			control_http_handler(request);
			return;
		}
		if (strcmp(request->query, "/") == 0) {
			home_http_handler(request);
			return;
		}
	}
	default_http_handler(request);
	return;
}


static void on_http_header_parser_fragment(http_header_parser_t *parser) {
	http_server_context_t *context = (http_server_context_t *)parser->handle;
	http_request_t *request = (http_request_t *)context->client_data;

	if (parser->header_finished) {
	}
	else if (parser->header_error) {
		ESP_LOGW(TAG, "http header not parsed");
	}
	else if (parser->key_length == -1 && parser->value_length != -1) {
		ESP_LOGD(TAG, "%s", parser->value);
		const char *query_pos = strstr(parser->value, " ");
		if (!query_pos) {
			ESP_LOGW(TAG, "http header not parsed");
			parser->header_error = true;
			return;
		}
		query_pos++;
		const char *version_pos = strstr(query_pos, " ");
		if (!version_pos) {
			ESP_LOGW(TAG, "http header not parsed");
			parser->header_error = true;
			return;
		}
		strncpy(request->method, parser->value, MIN(sizeof(request->method) - 1, query_pos - parser->value - 1));
		strncpy(request->query, query_pos, MIN(sizeof(request->query) - 1, version_pos - query_pos));
	}
	else {
		ESP_LOGD(TAG, "%s: %s", parser->key, parser->value);
		for (char *p = parser->key; *p; ++p) *p = tolower(*p);
		if (strcmp(parser->key, "upgrade") == 0) {
			strncpy(request->upgrade, parser->value, sizeof(request->upgrade) - 1);
		}
		if (strcmp(parser->key, "host") == 0) {
			strncpy(request->host, parser->value, sizeof(request->host) - 1);
		}
		if (strcmp(parser->key, "sec-websocket-key") == 0) {
			strncpy(request->sec_websocket_key, parser->value, sizeof(request->sec_websocket_key) - 1);
		}
	}
}


static void *on_http_event(http_event_type_t type, void *data) {
	switch (type) {
		case HTTP_OPEN:
			request.server = ((http_open_t *)data)->server;
			request.client_socket = ((http_open_t *)data)->client_socket;
			bzero(request.query, sizeof(request.query));
			bzero(request.method, sizeof(request.method));
			bzero(request.upgrade, sizeof(request.upgrade));
			bzero(request.host, sizeof(request.host));
			bzero(request.sec_websocket_key, sizeof(request.sec_websocket_key));
			return &request;
		case HTTP_REQUEST:
			xSemaphoreGive(player_status_semaphore);
			on_http_request((http_request_t *)data);
			break;
		case HTTP_CLOSE:
			xSemaphoreGive(request.write_data_semaphore);
			bzero(&request.server, sizeof(request.server));
			bzero(&request.client_socket, sizeof(request.client_socket));
			break;
		case HTTP_HEADER:
			on_http_header_parser_fragment((http_header_parser_t *)data);
			break;
	}
	return NULL;
}


static http_server_t server = {
	.task_name = "http_control",
	.port = CONFIG_HTTP_CONTROL_PORT,
};


static void http_control_init(void) {
	request.write_data_semaphore = xSemaphoreCreateBinary();
	if (request.write_data_semaphore == NULL) {
		ESP_LOGE(TAG, "write_data_semaphore not created");
	}
	player_status_semaphore = xSemaphoreCreateBinary();
	if (player_status_semaphore == NULL) {
		ESP_LOGE(TAG, "player_status_semaphore not created");
	}
	http_server_init(&server, on_http_event, NULL);
}


void http_control_task(void *arg) {
	http_control_init();
	while (1) {
		if (xSemaphoreTake(player_status_semaphore, portMAX_DELAY)) {
			while (1) {
				if (!request.server) {
					break;
				}
				xSemaphoreTake(request.write_data_semaphore, portMAX_DELAY);
				esp_err_t status = send_player_status(&request);
				xSemaphoreGive(request.write_data_semaphore);
				if (status != ESP_OK) {
					break;
				}
				vTaskDelay(50 / portTICK_PERIOD_MS);
			}
		}
	}
}


#endif
