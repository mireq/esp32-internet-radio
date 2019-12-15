#include <stdlib.h>
#include <string.h>

#include "source.h"


typedef struct uri_t {
	char *protocol;
	char *host;
	int port;
	char *path;
} uri_t;


uri_t *uri_parse(const char *uri) {
	const char *host_start = strstr(uri, "://");
	if (!host_start) {
		return NULL;
	}
	size_t protocol_length = host_start - uri;
	host_start += 3;
	const char *path_start = strstr(host_start, "/");
	if (!path_start) {
		return NULL;
	}
	uri_t *uri_parsed = (uri_t *)malloc(sizeof(uri_t));
	uri_parsed->protocol = (char *)malloc(protocol_length + 1);
	memcpy(uri_parsed->protocol, uri, protocol_length);
	uri_parsed->protocol[protocol_length] = '\0';
	uri_parsed->port = -1;
	const char *port_start = strstr(host_start, ":");
	if (port_start && port_start > path_start) {
		port_start = NULL;
	}
	size_t host_length = path_start - host_start;
	if (port_start) {
		host_length = port_start - host_start;
	}
	uri_parsed->host = (char *)malloc(host_length + 1);
	memcpy(uri_parsed->host, host_start, host_length);
	uri_parsed->host[host_length] = '\0';
	if (port_start) {
		port_start++;
		if (port_start < path_start) {
			uri_parsed->port = atoi(port_start);
		}
	}
	uri_parsed->path = strdup(path_start);
	if (uri_parsed->port == -1 && strcmp(uri_parsed->protocol, "http") == 0) {
		uri_parsed->port = 80;
	}
	return uri_parsed;
}

void uri_free(uri_t *uri) {
	if (!uri) {
		return;
	}
	if (uri->protocol) {
		free(uri->protocol);
	}
	if (uri->host) {
		free(uri->host);
	}
	if (uri->path) {
		free(uri->path);
	}
}


stream_t *stream_http_init(const uri_t *uri, stream_config callback);
void stream_http_destroy(stream_t *stream);


stream_t *stream_init(const char *uri, stream_config callback) {
	stream_t *stream = NULL;
	uri_t *uri_parsed = uri_parse(uri);
	if (strcmp(uri_parsed->protocol, "http") == 0) {
		stream = stream_http_init(uri_parsed, callback);
	}
	uri_free(uri_parsed);
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

stream_t *stream_http_init(const uri_t *uri, stream_config callback) {
	return NULL;
}

void stream_http_destroy(stream_t *stream) {

}
