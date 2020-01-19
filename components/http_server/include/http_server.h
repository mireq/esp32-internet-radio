#pragma once


#define HTTP_SERVER_QUERY_SIZE 64


struct http_server_t;


typedef enum {
	HTTP_OPEN,
	HTTP_REQUEST,
	HTTP_HEADER,
	HTTP_CLOSE,
} http_event_type_t;


typedef void *(*http_server_callback) (http_event_type_t type, void *data);


typedef struct http_server_t {
	void *handle;
	int port;
	http_server_callback callback;
	const char *task_name;
} http_server_t;


typedef struct http_open_t {
	http_server_t *server;
	int client_socket;
} http_open_t;


typedef struct http_server_context_t {
	http_server_t *server;
	void *client_data;
} http_server_context_t;


void http_server_init(http_server_t *server, http_server_callback callback, void *handle);
