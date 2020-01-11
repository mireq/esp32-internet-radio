#pragma once


#include <stdbool.h>


#define MAX_PLAYLIST_PATH_LENGTH 63
#define MAX_PLAYLIST_NAME_LENGTH 63
#define MAX_URI_SIZE 250


struct playlist_t;
struct playlist_item_t;

typedef void (*item_changed_callback) (struct playlist_t*, struct playlist_item_t *);

typedef char playlist_path_t[MAX_PLAYLIST_PATH_LENGTH + 1];
typedef struct playlist_item_t {
	playlist_path_t path;
	playlist_path_t next;
	playlist_path_t previous;
	playlist_path_t up;
	char name[MAX_PLAYLIST_NAME_LENGTH + 1];
	char uri[MAX_URI_SIZE + 1];
	bool loaded;
} playlist_item_t;


typedef struct playlist_t {
	playlist_item_t current;
	item_changed_callback callback;
	void *data;
} playlist_t;


void playlist_init(playlist_t *playlist, item_changed_callback callback, void *data);
void playlist_destroy(playlist_t *playlist);
void playlist_next(playlist_t *playlist);
void playlist_previous(playlist_t *playlist);
void playlist_up(playlist_t *playlist);
void playlist_open_current(playlist_t *playlist);
void playlist_cancel(playlist_t *playlist);
void playlist_get_current_path(playlist_t *playlist, char *buf);
void playlist_get_item(playlist_t *playlist, playlist_item_t *item);
