#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "http_header_parser/http_header_parser.h"
#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

#include "source.h"


static const char *TAG = "player_source";
#define MAX_HTTP_HEADER_SIZE 8192
char meta_value_buffer[MAX_META_VALUE_SIZE + 1];


static void uri_parse(uri_t *uri, const char *text) {
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
		if (port_start && i == port_start - text - 1) {
			pos++;
			i += (path_start - port_start);
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


static source_error_t source_http_init(source_t *source, const uri_t *uri);
static void source_http_destroy(source_t *source);
static ssize_t source_http_read(source_t *source, char *buf, ssize_t size);


source_error_t source_init(source_t *source, const char *uri) {
	source_error_t status = SOURCE_NO_ERROR;
	bzero(source->content_type, sizeof(source->content_type));
	source->metadata_callback = NULL;
	uri_t uri_parsed;
	uri_parse(&uri_parsed, uri);
	if (!uri_parsed.protocol) {
		status = SOURCE_URI_ERROR;
	}
	else if (strcmp(uri_parsed.protocol, "http") == 0) {
		status = source_http_init(source, &uri_parsed);
	}
	else {
		status = SOURCE_URI_ERROR;
	}
	return status;
}


void source_destroy(source_t *source) {
	switch (source->type) {
		case SOURCE_TYPE_UNKNOWN:
			break;
		case SOURCE_TYPE_HTTP:
			source_http_destroy(source);
			break;
	}
}


ssize_t source_read(source_t *source, char *buf, ssize_t size) {
	switch (source->type) {
		case SOURCE_TYPE_UNKNOWN:
			return -1;
		case SOURCE_TYPE_HTTP:
			return source_http_read(source, buf, size);
		default:
			return -1;
	}
}


/* ===== HTTP source ===== */

typedef struct http_header_t {
	int status;
	int icy_metaint;
	char content_type[HTTP_HEADER_VALUE_BUFFER_SIZE];
} http_header_t;


static void on_http_header_parser_fragment(http_header_parser_t *parser) {
	http_header_t *header = (http_header_t *)parser->handle;

	if (parser->header_finished) {
	}
	else if (parser->header_error) {
		header->status = -1;
		ESP_LOGW(TAG, "http header not parsed");
	}
	else if (parser->key_length == -1 && parser->value_length == -1) {
		ESP_LOGD(TAG, "http status: %d", parser->status);
		if (header->status == 0) {
			header->status = parser->status;
		}
	}
	else {
		ESP_LOGD(TAG, "header %s: %s", parser->key, parser->value);
		for (char *p = parser->key; *p; ++p) *p = tolower(*p);
		if (strcmp(parser->key, "content-type") == 0) {
			strcpy(header->content_type, parser->value);
		}
		if (strcmp(parser->key, "icy-metaint") == 0) {
			header->icy_metaint = atoi(parser->value);
		}
	}
}


static source_error_t source_http_init(source_t *source, const uri_t *uri) {
	xSemaphoreTake(source->semaphore, portMAX_DELAY);
	ESP_LOGI(TAG, "opening source http://%s:%d%s", uri->host, uri->port, uri->path);
	source_data_http_t *http = &source->data.http;
	source->type = SOURCE_TYPE_HTTP;
	http->icy_meta_interval = 0;
	http->icy_meta_interval_distance = 0;
	http->icy_meta_size = 0;
	http->icy_meta_readed = 0;
	http->sock = -1;

	const struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
	};

	struct addrinfo *res;
	char port[6];
	snprintf(port, 6, "%d", uri->port);
	int err = getaddrinfo(uri->host, port, &hints, &res);
	if (err != 0 || !res) {
		ESP_LOGE(TAG, "dns lookup failed for host %s err=%d res=%p", uri->host, err, res);
		if (res) {
			freeaddrinfo(res);
		}
		xSemaphoreGive(source->semaphore);
		return SOURCE_READING_ERROR;
	}

	int sock = socket(res->ai_family, res->ai_socktype, 0);
	if (sock < 0) {
		ESP_LOGE(TAG, "failed to allocate socket.");
		freeaddrinfo(res);
		xSemaphoreGive(source->semaphore);
		return SOURCE_READING_ERROR;
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
		ESP_LOGE(TAG, "failed to connnect to socket. err=%d", errno);
		close(sock);
		freeaddrinfo(res);
		xSemaphoreGive(source->semaphore);
		return SOURCE_READING_ERROR;
	}
	freeaddrinfo(res);
	xSemaphoreGive(source->semaphore);

	const char template[] = "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: ESP32\r\nAccept: */*\r\nIcy-MetaData: 1\r\n\r\n";
	char request[MAX_URI_SIZE + sizeof(template) + 1];
	bzero(request, sizeof(request));
	sprintf(request, template, uri->path, uri->host);

	if (write(sock, request, strlen(request)) < 0) {
		ESP_LOGE(TAG, "socket write failed");
		close(sock);
		return SOURCE_READING_ERROR;
	}

	http_header_t http_header = {
		.status = 0,
		.icy_metaint = 0,
		.content_type = "",
	};
	http_header_parser_t parser;
	http_header_parser_init(&parser, on_http_header_parser_fragment, &http_header);

	char c;
	for (size_t i = 0; i < MAX_HTTP_HEADER_SIZE; ++i) {
		ssize_t received = -1;
		while (received == -1) {
			received = recv(sock, &c, sizeof(c), 0);
			if (received == -1 && errno != EINTR) {
				break;
			}
		}
		http_header_parser_feed(&parser, c);
		if (parser.header_finished || parser.header_error) {
			break;
		}
	}

	if (http_header.status != 200) {
		ESP_LOGE(TAG, "wrong http status, excepted 200, got %d", http_header.status);
		close(sock);
		return SOURCE_READING_ERROR;
	}

	http->sock = sock;
	http->icy_meta_interval = http_header.icy_metaint;
	http->icy_meta_interval_distance = http_header.icy_metaint;
	strcpy(source->content_type, http_header.content_type);

	ESP_LOGI(TAG, "opened http stream");

	return SOURCE_NO_ERROR;
}

static void source_http_destroy(source_t *source) {
	xSemaphoreTake(source->semaphore, portMAX_DELAY);
	source_data_http_t *http = &source->data.http;
	if (http->sock >= 0) {
		close(http->sock);
		http->sock = -1;
		ESP_LOGI(TAG, "closed http stream");
	}
	xSemaphoreGive(source->semaphore);
}

static void parse_icy_metadata(source_t *source, char *buf) {
	buf[MAX_ICY_SIZE - 1] = '\0';
	char *title = strstr(buf, "StreamTitle=");
	if (title == NULL) {
		return;
	}
	title += sizeof("StreamTitle=");
	char *title_end = title;
	title_end = strstr(title_end, ";");
	if (title_end == NULL) {
		return;
	}
	title_end--;

	if (title_end > title) {
		bzero(meta_value_buffer, sizeof(meta_value_buffer));
		strncpy(meta_value_buffer, title, title_end - title);
		if (source->metadata_callback) {
			source->metadata_callback(source, "title", meta_value_buffer);
		}
	}
}

static ssize_t source_http_read_icy_metadata(source_t *source) {
	source_data_http_t *http = &source->data.http;
	ssize_t received = -1;
	ssize_t requested = http->icy_meta_size;
	uint8_t size;
	char *buf;
	if (requested == -1) {
		buf = (char *)(&size);
		requested = 1;
		bzero(http->icy_meta_buffer, sizeof(http->icy_meta_buffer));
		http->icy_meta_interval_distance = http->icy_meta_interval;
	}
	else {
		buf = &http->icy_meta_buffer[http->icy_meta_readed];
		requested = http->icy_meta_size - http->icy_meta_readed;
	}
	while (received == -1) {
		received = recv(source->data.http.sock, buf, requested, 0);
		if (received == -1 && errno != EINTR) {
			if (errno == EAGAIN) {
				received = 0;
			}
			else if (errno != EINTR) {
				break;
			}
		}
	}
	if (received > 0) {
		if (http->icy_meta_size == -1) {
			http->icy_meta_size = size * 16;
		}
		else {
			http->icy_meta_readed += received;
			if (http->icy_meta_readed == http->icy_meta_size) {
				parse_icy_metadata(source, http->icy_meta_buffer);
				http->icy_meta_size = 0;
				http->icy_meta_readed = 0;
			}
		}

		return SOURCE_READ_AGAIN;
	}
	return received;
}

static ssize_t source_http_read(source_t *source, char *buf, ssize_t size) {
	source_data_http_t *http = &source->data.http;
	ssize_t received = -1;
	ssize_t requested = size;
	if (http->icy_meta_size != 0) {
		return source_http_read_icy_metadata(source);
	}
	if (http->icy_meta_interval > 0 && requested > http->icy_meta_interval_distance) {
		requested = http->icy_meta_interval_distance;
		if (requested == 0) {
			http->icy_meta_size = -1;
			http->icy_meta_readed = 0;
			return source_http_read_icy_metadata(source);
		}
	}
	while (received == -1) {
		received = recv(source->data.http.sock, buf, requested, MSG_DONTWAIT);
		if (received == -1 && errno != EINTR) {
			if (errno == EAGAIN) {
				received = 0;
			}
			else if (errno != EINTR) {
				break;
			}
		}
	}
	if (received > 0) {
		http->icy_meta_interval_distance -= received;
	}
	return received;
}
