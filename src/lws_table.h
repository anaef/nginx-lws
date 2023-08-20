/*
 * LWS table
 *
 * Copyright (C) 2023 Andre Naef
 */


#ifndef _LWS_TABLE_INCLUDED
#define _LWS_TABLE_INCLUDED


#include <ngx_config.h>
#include <ngx_core.h>


typedef struct lws_table_s lws_table_t;
typedef struct lws_table_entry_s lws_table_entry_t;

struct lws_table_s {
	size_t               alloc;      /* allocated slots */
	size_t               load;       /* load limit for rehash */
	size_t               count;      /* number of entries */
	lws_table_entry_t   *entries;    /* entries */
	ngx_queue_t          order;      /* insert order; LRU if capped */
	time_t               timeout;    /* timeout of entries */
	size_t               cap;        /* cap */
	unsigned             dup:1;      /* duplicate keys */
	unsigned             free:1;     /* free values */
	unsigned             ci:1;       /* case insensitive */
	unsigned             timed:1;    /* with timeout */
	unsigned             capped:1;   /* capped, e.g., for caches */
};

typedef enum {
	LWS_TES_UNUSED,
	LWS_TES_SET,
	LWS_TES_DELETED
} lws_table_entry_state_e;

struct lws_table_entry_s {
	ngx_queue_t              order;  /* see above */
	ngx_str_t                key;    /* key; managed if dup is set */
	void                    *value;  /* value; managed if free is set */
	ngx_uint_t               hash;   /* key hash */
	time_t                   time;   /* base for entry timeout; if timed is set */
	lws_table_entry_state_e  state;  /* state [unused, set, deleted] */
};


lws_table_t *lws_table_create(size_t alloc);
void lws_table_free(lws_table_t *t);
void lws_table_clear(lws_table_t *t);
int lws_table_set_dup(lws_table_t *t, int dup);
int lws_table_set_free(lws_table_t *t, int free);
int lws_table_set_ci(lws_table_t *t, int ci);
int lws_table_set_timeout(lws_table_t *t, time_t timeout);
int lws_table_set_cap(lws_table_t *t, size_t cap);
void *lws_table_get(lws_table_t *t, ngx_str_t *key);
int lws_table_set(lws_table_t *t, ngx_str_t *key, void *value);
int lws_table_next(lws_table_t *t, ngx_str_t *key, ngx_str_t **next, void **value);


#define lws_table_empty(t)  ((t)->count == 0)
#define lws_table_count(t)  (t)->count


#endif /* _LWS_TABLE_INCLUDED */
