#include <string.h>
#include <strings.h>

#include "sdkconfig.h"
#include "playlist.h"


static void clear_playlist_item(playlist_item_t *item) {
	bzero(item->path, sizeof(item->path));
	bzero(item->next, sizeof(item->next));
	bzero(item->previous, sizeof(item->previous));
	bzero(item->up, sizeof(item->up));
	bzero(item->name, sizeof(item->name));
	bzero(item->uri, sizeof(item->uri));
	item->loaded = false;
}


static void set_playlist_item(playlist_item_t *item, const char *path, const char *next, const char *previous, const char *up, const char *name, const char *uri, bool loaded) {
	strncpy(item->path, path, sizeof(item->path));
	strncpy(item->next, next, sizeof(item->next));
	strncpy(item->previous, previous, sizeof(item->previous));
	strncpy(item->up, up, sizeof(item->up));
	strncpy(item->name, name, sizeof(item->name));
	strncpy(item->uri, uri, sizeof(item->uri));
	item->loaded = loaded;
}


void playlist_init(playlist_t *playlist, item_changed_callback callback, void *data) {
	clear_playlist_item(&playlist->current);
	playlist->callback = callback;
	playlist->data = data;
}


void playlist_destroy(playlist_t *playlist) {
	playlist_cancel(playlist);
	clear_playlist_item(&playlist->current);
}

void playlist_next(playlist_t *playlist) {
}

void playlist_previous(playlist_t *playlist) {
}

void playlist_up(playlist_t *playlist) {
}

void playlist_open_current(playlist_t *playlist) {
	if (!playlist->current.loaded) {
		set_playlist_item(
			&playlist->current,
			"", "", "", "",
			CONFIG_PLAYLIST_DEFAULT_STATION_NAME,
			CONFIG_PLAYLIST_DEFAULT_STATION_URL,
			true
		);
	}
	if (playlist->callback) {
		playlist->callback(playlist, &playlist->current);
	}
}

void playlist_cancel(playlist_t *playlist) {
	if (playlist->current.loaded) {
		clear_playlist_item(&playlist->current);
		if (playlist->callback) {
			playlist->callback(playlist, &playlist->current);
		}
	}
}

void playlist_get_current_path(playlist_t *playlist, char *buf) {
	strncpy(buf, playlist->current.path, sizeof(playlist->current.path) - 1);
}

void playlist_get_item(playlist_t *playlist, playlist_item_t *item) {
	memcpy(item, &playlist->current, sizeof(playlist->current));
}

void playlist_set_item_simple(playlist_t *playlist, const char *name, const char *uri) {
	strncpy(playlist->current.name, name, sizeof(playlist->current.name));
	strncpy(playlist->current.uri, uri, sizeof(playlist->current.uri));
	playlist->current.loaded = true;
	if (playlist->callback) {
		playlist->callback(playlist, &playlist->current);
	}
}
