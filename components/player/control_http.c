#include <stdio.h>
#include <strings.h>

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "mbedtls/base64.h"
#include "mbedtls/sha1.h"

#include "control_http.h"
#include "http_server.h"


#if CONFIG_HTTP_CONTROL


static const char http_not_found[] = "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n";
static const char http_found[] = "HTTP/1.1 302 Found\r\nLocation: %s?device=%s\r\nConnection: close\r\n\r\n";
static const char http_ws_start[] = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ";
static const char ws_magic_guid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";


static void ws_handshake(http_request_t *request) {
	char key[64];
	strcpy(key, request->sec_websocket_key);
	strcpy(key + strlen(key), ws_magic_guid);

	unsigned char sha1sum[20];
	mbedtls_sha1((unsigned char *) key, strlen(key), sha1sum);
	size_t olen;
	mbedtls_base64_encode(key, sizeof(key), &olen, sha1sum, 20);
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


static void control_http_handler(http_request_t *request) {
	ws_handshake(request);
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
	static http_request_t request;
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
