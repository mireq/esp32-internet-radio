#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "sdkconfig.h"

#include "http_header_parser/http_header_parser.h"
#include "http_server.h"


#define MAX_HTTP_HEADER_SIZE 1024


static const char *TAG = "http_server";


static void on_http_header_parser_fragment(http_header_parser_t *parser) {
	http_server_context_t *context = (http_server_context_t *)parser->handle;
	context->server->callback(HTTP_HEADER, parser);
}


void http_server(void *args) {
	http_server_t *server = (http_server_t *)args;
	int socketfd, connection;
	socklen_t clientaddr_len;
	struct sockaddr_in servaddr, clientaddr;

	socketfd = socket(AF_INET, SOCK_STREAM, 0);
	if (socketfd == -1) {
		ESP_LOGE(TAG, "Socket not allocated");
		vTaskDelete(NULL);
		return;
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(server->port);

	if ((bind(socketfd, (struct sockaddr*)&servaddr, sizeof(servaddr))) != 0) {
		ESP_LOGE(TAG, "Socket bind failed");
		close(socketfd);
		vTaskDelete(NULL);
		return;
	}

	if (listen(socketfd, 1) != 0) {
		ESP_LOGE(TAG, "Listen failed");
		close(socketfd);
		vTaskDelete(NULL);
		return;
	}

	clientaddr_len = sizeof(clientaddr);
	while (1) {
		connection = accept(socketfd, (struct sockaddr*)&clientaddr, &clientaddr_len);
		void *client_data;

		// Open connection
		{
			http_open_t open = {
				.server = server,
				.client_socket = connection,
			};
			client_data = server->callback(HTTP_OPEN, &open);
		}

		http_server_context_t context = {
			.server = server,
			.client_data = client_data,
		};
		// Parse header
		bool http_error = false;
		{
			http_header_parser_t parser;
			http_header_parser_init(&parser, on_http_header_parser_fragment, &context);
			parser.request = 1;
			char c;
			for (size_t i = 0; i < MAX_HTTP_HEADER_SIZE; ++i) {
				ssize_t received = -1;
				while (received == -1) {
					received = recv(connection, &c, sizeof(c), 0);
					if (received == -1 && errno != EINTR) {
						http_error = true;
						break;
					}
				}
				http_header_parser_feed(&parser, c);
				if (parser.header_error) {
					http_error = true;
				}
				if (parser.header_finished || parser.header_error) {
					break;
				}
			}
		}

		if (!http_error) {
			server->callback(HTTP_REQUEST, client_data);
		}

		server->callback(HTTP_CLOSE, client_data);
		close(connection);
	}

	close(socketfd);
	vTaskDelete(NULL);
}


void http_server_init(http_server_t *server, http_server_callback callback, void *handle) {
	server->handle = handle;
	server->callback = callback;
	if (xTaskCreatePinnedToCore(&http_server, server->task_name, configMINIMAL_STACK_SIZE + 2048, server, tskIDLE_PRIORITY + 1, NULL, 0) != pdPASS) {
		ESP_LOGE(TAG, "HTTP server task not initialized");
	}
}
