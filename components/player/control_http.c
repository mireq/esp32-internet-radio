#include <stdbool.h>
#include <stdio.h>
#include <strings.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "mbedtls/base64.h"
#include "mbedtls/sha1.h"

#include "control_http.h"
#include "http_server.h"
#include "interface.h"


#if CONFIG_HTTP_CONTROL

#define MAX_WS_FRAME_SIZE 125


typedef struct websocket_frame_t {
	char data[MAX_WS_FRAME_SIZE];
	uint16_t size;
} websocket_frame_t;


typedef struct command_message_t {
	uint16_t command;
	char data[MAX_WS_FRAME_SIZE - 2];
	uint16_t size;
} command_message_t;
typedef command_message_t response_message_t;



static const char http_not_found[] = "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n";
static const char http_found[] = "HTTP/1.1 302 Found\r\nLocation: %s?device=%s\r\nConnection: close\r\n\r\n";
static const char http_ws_start[] = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ";
static const char ws_magic_guid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

static http_request_t request;


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


static esp_err_t receive_websocket_frame(http_request_t *request, websocket_frame_t *frame) {
	uint16_t header;
	char mask[] = {0, 0, 0, 0};
	ssize_t received;
	received = recv(request->client_socket, &header, sizeof(header), MSG_WAITALL);
	if (received == -1) {
		return ESP_FAIL;
	}
	bool has_mask = false;
	if (header & 0x80) {
		has_mask = true;
	}
	uint8_t opcode = header & 0x0f;
	if (opcode == 0x08) { // Final frame
		return ESP_FAIL;
	}

	frame->size = (header >> 8) & 0x7f;

	// Message too long
	if (frame->size > MAX_WS_FRAME_SIZE) {
		return ESP_FAIL;
	}

	// Read optional mask
	if (has_mask) {
		received = recv(request->client_socket, &mask, sizeof(mask), MSG_WAITALL);
		if (received == -1) {
			return ESP_FAIL;
		}
	}

	// Read message
	received = recv(request->client_socket, frame->data, frame->size, MSG_WAITALL);
	if (received == -1) {
		return ESP_FAIL;
	}

	// Apply mask
	for (size_t i = 0; i < frame->size; ++i) {
		frame->data[i] ^= mask[i & 0x03];
	}

	return ESP_OK;
}


static esp_err_t send_websocket_frame(http_request_t *request, const websocket_frame_t *frame) {
	char buf[MAX_WS_FRAME_SIZE + 2];
	buf[0] = 0x80 | 0x02; // binary
	buf[1] = frame->size;
	memcpy(buf + 2, frame->data, frame->size);
	ssize_t processed;
	processed = send(request->client_socket, buf, frame->size + 2, 0);
	if (processed < 0) {
		return ESP_FAIL;
	}
	return ESP_OK;
}


static esp_err_t send_websocket_response(http_request_t *request, const response_message_t *message) {
	websocket_frame_t wsframe;
	wsframe.size = message->size + sizeof(message->command);
	memcpy(&wsframe.data, &message->command, sizeof(message->command));
	memcpy(&wsframe.data + sizeof(message->command), message->data, message->size);
	send_websocket_frame(request, &wsframe);
	return ESP_OK;
}


static esp_err_t handle_ping(http_request_t *request, command_message_t *message) {
	static const response_message_t response = {
		.command = WS_RESPONSE_PONG,
		.size = 0,
	};
	send_websocket_response(request, &response);
	return ESP_OK;
}


static esp_err_t handle_set_playlist_item(http_request_t *request, command_message_t *message) {
	char *uri = message->data;
	char *name = message->data;
	name += strlen(uri) + 1;
	playlist_set_item_simple(&playlist, name, uri);
	return ESP_OK;
}


static esp_err_t process_websocket_message(http_request_t *request, command_message_t *message) {
	switch (message->command) {
		case WS_COMMAND_PING:
			return handle_ping(request, message);
		case WS_COMMAND_SET_PLAYLIST_ITEM:
			return handle_set_playlist_item(request, message);
		default:
			return ESP_FAIL;
	}
}


static void control_http_handler(http_request_t *request) {
	ws_handshake(request);
	while (1) {
		websocket_frame_t websocket_frame;
		if (receive_websocket_frame(request, &websocket_frame) != ESP_OK) {
			break;
		}
		command_message_t *msg = (command_message_t *)&websocket_frame;
		msg->size -= sizeof(msg->command);
		if (process_websocket_message(request, msg) != ESP_OK) {
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
			on_http_request((http_request_t *)data);
			break;
		case HTTP_CLOSE:
			bzero(&request, sizeof(request));
			break;
	}
	return NULL;
}


static http_server_t server = {
	.task_name = "http_control",
	.port = CONFIG_HTTP_CONTROL_PORT,
};


void init_http_control(void) {
	http_server_init(&server, on_http_event, NULL);
}


#endif
