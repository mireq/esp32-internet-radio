#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "esp_log.h"
#include "http_header_parser/http_header_parser.h"
#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

#include "source.h"

#define TAG "player_source"
#define MAX_HTTP_HEADER_SIZE 8192
#define MAX_URI_SIZE 250


typedef struct uri_t {
	char uri[MAX_URI_SIZE + 3];
	char *protocol;
	char *host;
	int port;
	char *path;
} uri_t;


typedef struct http_stream_metadata_t {
	int icy_meta_interval;
} http_stream_metadata_t;


void uri_parse(uri_t *uri, const char *text) {
	char c;
	uri->protocol = NULL;
	uri->port = -1;
	uri->host = NULL;
	uri->path = NULL;

	const char *host_start = strstr(text, "://");
	if (!host_start) {
		return;
	}
	host_start += 3;
	const char *path_start = strstr(host_start, "/");
	if (!path_start) {
		return;
	}
	const char *port_start = strstr(host_start, ":");
	if (port_start && port_start > path_start) {
		port_start = NULL;
	}
	if (port_start) {
		port_start++;
		if (port_start < path_start) {
			uri->port = atoi(port_start);
		}
	}

	char *pos = uri->uri;
	uri->protocol = pos;
	for (size_t i = 0; i < MAX_URI_SIZE; ++i) {
		c = text[i];
		if (c == '\0') {
			break;
		}
		if (i == host_start - text - 3) {
			pos++;
			i += 2;
			uri->host = pos;
			continue;
		}
		if (i == path_start - text) {
			pos++;
			uri->path = pos;
		}
		*pos = c;
		pos++;
		*pos = '\0';
	}

	if (uri->port == -1 && strcmp(uri->protocol, "http") == 0) {
		uri->port = 80;
	}
}


stream_t *stream_http_init(const uri_t *uri, stream_config callback);
void stream_http_destroy(stream_t *stream);


stream_t *stream_init(const char *uri, stream_config callback) {
	stream_t *stream = NULL;
	uri_t uri_parsed;
	uri_parse(&uri_parsed, uri);
	if (!uri_parsed.protocol) {
		return stream;
	}
	if (strcmp(uri_parsed.protocol, "http") == 0) {
		stream = stream_http_init(&uri_parsed, callback);
	}
	return stream;
}

void stream_destroy(stream_t *stream) {
	if (!stream) {
		return;
	}
	switch (stream->type) {
		case STREAM_TYPE_HTTP:
			stream_http_destroy(stream);
			break;
	}
}


/* ===== HTTP stram ===== */

void on_http_header_parser_fragment(http_header_parser_t *parser) {
	if (parser->header_finished) {
		printf("End header");
	}
	else if (parser->header_error) {
		printf("Header error");
	}
	else if (parser->key_length == -1 && parser->value_length == -1) {
		printf("Status: %d\n", parser->status);
	}
	else {
		printf("Key: %s, Value: %s\n", parser->key, parser->value);
	}
}


stream_t *stream_http_init(const uri_t *uri, stream_config callback) {
	const struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
	};

	printf("%s %d %s %s", uri->protocol, uri->port, uri->host, uri->path);
	struct addrinfo *res;
	char port[6];
	snprintf(port, 6, "%d", uri->port);
	int err = getaddrinfo(uri->host, port, &hints, &res);
	if (err != 0 || !res) {
		ESP_LOGE(TAG, "DNS lookup failed for host %s err=%d res=%p", uri->host, err, res);
		if (res) {
			freeaddrinfo(res);
		}
		return NULL;
	}

	int sock = socket(res->ai_family, res->ai_socktype, 0);
	if (sock < 0) {
		ESP_LOGE(TAG, "Failed to allocate socket.");
		freeaddrinfo(res);
		return NULL;
	}

	int retries = 10;
	err = -1;
	while (err != 0 && retries > 0) {
		err = connect(sock, res->ai_addr, res->ai_addrlen);
		if (errno != EINTR) {
			retries -= 1;
		}
	}
	if (err != 0) {
		ESP_LOGE(TAG, "Failed to open socket. err=%d", errno);
		close(sock);
		freeaddrinfo(res);
		return NULL;
	}
	freeaddrinfo(res);

	char *request;
	if (asprintf(&request, "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: ESP32\r\nAccept: */*\r\nIcy-MetaData: 1\r\n\r\n", uri->path, uri->host) < 0) {
		close(sock);
		return NULL;
	}

	if (write(sock, request, strlen(request)) < 0) {
		ESP_LOGE(TAG, "Socket write failed");
		close(sock);
		free(request);
		return NULL;
	}
	free(request);

	http_stream_metadata_t metadata;
	http_header_parser_t parser;
	http_header_parser_init(&parser, on_http_header_parser_fragment, &metadata);

	char c;
	for (size_t i = 0; i < MAX_HTTP_HEADER_SIZE; ++i) {
		ssize_t received = -1;
		while (received == -1) {
			received = read(sock, &c, sizeof(c));
			if (received == -1 && errno != EINTR) {
				break;
			}
		}
		http_header_parser_feed(&parser, c);
		if (parser.header_finished || parser.header_error) {
			break;
		}
		//printf("%c", c);
	}
	printf("\n");

	return NULL;
}

void stream_http_destroy(stream_t *stream) {
}
