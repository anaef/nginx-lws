/*
 * LWS table
 *
 * Copyright (C) 2023 Andre Naef
 */


#include <lws_table.h>


static ngx_uint_t lws_table_hash(lws_table_t *t, ngx_str_t *key);
static size_t lws_table_size(lws_table_t *t, size_t size);
static size_t lws_table_load(lws_table_t *t, size_t alloc);
static int lws_table_rehash(lws_table_t *t, size_t alloc);
static lws_table_entry_t *lws_table_find(lws_table_t *t, ngx_str_t *key, ngx_uint_t hash);
static lws_table_entry_t *lws_table_insert(lws_table_t *t, ngx_str_t *key, ngx_uint_t hash);
static void lws_table_remove(lws_table_t *t, lws_table_entry_t *entry);


#if SIZE_MAX > UINT32_MAX
static size_t lws_table_sizes[] = {
	3, 5, 7, 11, 13, 17, 23, 29, 41, 53, 67, 89, 127, 157, 211, 277, 373, 499, 659, 877, 1171,
	1553, 2081, 2767, 3691, 4909, 6547, 8731, 11633, 15511, 20681, 27581, 36749, 49003, 65353,
	87107, 116141, 154871, 206477, 275299, 367069, 489427, 652559, 870083, 1160111, 1546799,
	2062391, 2749847, 3666461, 4888619, 6518173, 8690917, 11587841, 15450437, 20600597,
	27467443, 36623261, 48831017, 65107997, 86810681, 115747549, 154330079, 205773427,
	274364561, 365819417, 487759219, 650345651, 867127501, 1156170011, 1541560037, 2055413317,
	2740551103, 3654068141, 4872090871, 6496121063, 8661494753, 11548659701, 15398212901,
	20530950533, 27374600677, 36499467569, 48665956771, 64887942367, 86517256433,
	115356341911, 153808455923, 205077941191, 273437254897, 364583006561, 486110675443,
	648147567293, 864196756231, 1152262341641, 1536349788871, 2048466385123, 2731288513529,
	3641718017983, 4855624023953, 6474165365293, 8632220487029, 11509627316059,
	15346169754719, 20461559672951, 27282079563967, 36376106085223, 48501474780299,
	64668633040457, 86224844053847, 114966458738489, 153288611651291, 204384815535079,
	272513087380099
};
static int lws_table_sizes_n = 112;
#else
static size_t lws_table_sizes[] = {
	3, 5, 7, 11, 13, 17, 23, 29, 41, 53, 67, 89, 127, 157, 211, 277, 373, 499, 659, 877, 1171,
	1553, 2081, 2767, 3691, 4909, 6547, 8731, 11633, 15511, 20681, 27581, 36749, 49003, 65353,
	87107, 116141, 154871, 206477, 275299, 367069, 489427, 652559, 870083, 1160111, 1546799,
	2062391, 2749847, 3666461, 4888619, 6518173, 8690917, 11587841, 15450437, 20600597,
	27467443, 36623261, 48831017, 65107997, 86810681, 115747549, 154330079, 205773427,
	274364561, 365819417, 487759219, 650345651, 867127501, 1156170011, 1541560037, 2055413317,
	2740551103, 3654068141
};
static int lws_table_sizes_n = 73;
#endif

lws_table_t *lws_table_create (size_t load, ngx_log_t *log) {
	size_t        alloc;
	lws_table_t  *t;

	t = ngx_calloc(sizeof(lws_table_t), log);
	if (!t) {
		return NULL;
	}
	t->log = log;
	alloc = load + (load < lws_table_sizes[lws_table_sizes_n - 1] ? load / 7 + 3 : 0);
	if ((t->alloc = lws_table_size(t, alloc)) == (size_t)-1) {
		ngx_free(t);
		return NULL;
	}
	t->load = lws_table_load(t, t->alloc);  /* t->load >= load */
	t->entries = ngx_calloc(t->alloc * sizeof(lws_table_entry_t), t->log);
	if (!t->entries) {
		ngx_free(t);
		return NULL;
	}
	ngx_queue_init(&t->order);
	return t;
}

void lws_table_free (lws_table_t *t) {
	ngx_queue_t        *q;
	lws_table_entry_t  *entry;

	if (t->dup || t->free) {
		while (!ngx_queue_empty(&t->order)) {
			q = ngx_queue_last(&t->order);
			entry = ngx_queue_data(q, lws_table_entry_t, order);
			lws_table_remove(t, entry);
		}
	}
	ngx_free(t->entries);
	ngx_free(t);
}

void lws_table_clear (lws_table_t *t) {
	ngx_queue_t        *q;
	lws_table_entry_t  *entry;

	if (t->dup || t->free) {
		while (!ngx_queue_empty(&t->order)) {
			q = ngx_queue_last(&t->order);
			entry = ngx_queue_data(q, lws_table_entry_t, order);
			lws_table_remove(t, entry);
		}
	} else {
		ngx_queue_init(&t->order);
		t->count = 0;
	}
	ngx_memzero(t->entries, t->alloc * sizeof(lws_table_entry_t));
}

int lws_table_set_dup (lws_table_t *t, int dup) {
	if (t->count) {
		return -1;
	}
	t->dup = dup;
	return 0;
}

int lws_table_set_free (lws_table_t *t, int free) {
	if (t->count) {
		return -1;
	}
	t->free = free;
	return 0;
}

int lws_table_set_ci (lws_table_t *t, int ci) {
	if (t->count) {
		return -1;
	}
	t->ci = ci;
	return 0;
}

int lws_table_set_timeout (lws_table_t *t, time_t timeout) {
	if (t->count) {
		return -1;
	}
	t->timeout = timeout;
	t->timed = 1;
	return 0;
}

int lws_table_set_cap (lws_table_t *t, size_t cap) {
	if (t->count || t->cap < 1) {
		return -1;
	}
	t->cap = cap;
	t->capped = 1;
	return 0;
}

void *lws_table_get (lws_table_t *t, ngx_str_t *key) {
	ngx_uint_t          hash;
	lws_table_entry_t  *entry;

	hash = lws_table_hash(t, key);
	entry = lws_table_find(t, key, hash);
	if (!entry) {
		return NULL;
	}
	if (t->timed) {
		if (entry->time + t->timeout <= time(NULL)) {
			return NULL;
		}
	}
	if (t->capped) {
		ngx_queue_remove(&entry->order);
		ngx_queue_insert_tail(&t->order, &entry->order);
	}
	return entry->value;
}

int lws_table_set (lws_table_t *t, ngx_str_t *key, void *value) {
	ngx_uint_t          hash;
	ngx_queue_t        *q;
	lws_table_entry_t  *entry, *evict;

	hash = lws_table_hash(t, key);
	entry = lws_table_find(t, key, hash);
	if (value) {
		if (entry) {
			/* update existing */
			if (t->capped) {
				ngx_queue_remove(&entry->order);
				ngx_queue_insert_tail(&t->order, &entry->order);
			}
			if (t->free && value != entry->value) {
				ngx_free(entry->value);
			}
			entry->value = value;
		} else {
			/* evict as needed */
			if (t->capped && t->count == t->cap) {
				q = ngx_queue_head(&t->order);
				evict = ngx_queue_data(q, lws_table_entry_t, order);
				lws_table_remove(t, evict);
			}

			/* rehash as needed */
			if (t->count == t->load) {
				if (lws_table_rehash(t, t->alloc + 1) != 0) {
					return -1;
				}
			}

			/* new entry */
			entry = lws_table_insert(t, key, hash);
			if (t->dup) {
				entry->key.data = ngx_alloc(key->len, t->log);
				if (!entry->key.data) {
					return -1;
				}
				ngx_memcpy(entry->key.data, key->data, key->len);
				entry->key.len = key->len;
			} else {
				entry->key = *key;
			}
			entry->value = value;
			entry->hash = hash;
			entry->state = LWS_TES_SET;
			ngx_queue_insert_tail(&t->order, &entry->order);
			t->count++;
		}
		if (t->timed) {
			entry->time = time(NULL);
		}
	} else {
		/* remove */
		if (entry) {
			lws_table_remove(t, entry);
		}
	}
	return 0;
}

int lws_table_next (lws_table_t *t, ngx_str_t *key, ngx_str_t **next, void **value) {
	ngx_uint_t          hash;
	ngx_queue_t        *q;
	lws_table_entry_t  *entry;

	if (key) {
		/* continuation */
		hash = lws_table_hash(t, key);
		entry = lws_table_find(t, key, hash);
		if (!entry) {
			return -1;
		}
		q = ngx_queue_next(&entry->order);
	} else {
		/* start */
		q = ngx_queue_head(&t->order);
	}
	if (q == ngx_queue_sentinel(&t->order)) {
		return -1;
	}
	entry = ngx_queue_data(q, lws_table_entry_t, order);
	*next = &entry->key;
	*value = entry->value;
	return 0;
}

static ngx_uint_t lws_table_hash (lws_table_t *t, ngx_str_t *key) {
	u_char     *p;
	ngx_uint_t  hash;

	/* FNV-1a; source: http://www.isthe.com/chongo/tech/comp/fnv/index.html#FNV-1a */
	#if SIZE_MAX > UINT32_MAX
	#define _LWS_TABLE_HASH_OFFSET_BASIS 14695981039346656037U
	#define _LWS_TABLE_HASH_PRIME 1099511628211
	#else
	#define _LWS_TABLE_HASH_OFFSET_BASIS 2166136261U
	#define _LWS_TABLE_HASH_PRIME 16777619
	#endif
	hash = _LWS_TABLE_HASH_OFFSET_BASIS;
	p = key->data + key->len;
	if (t->ci) {
		while (p > key->data) {
			p--;
			hash ^= ngx_tolower(*p);  /* no '--' due to macro multiple evaluation */
			hash *= _LWS_TABLE_HASH_PRIME;
		}
	} else {
		while (p > key->data) {
			hash ^= *--p;
			hash *= _LWS_TABLE_HASH_PRIME;
		}
	}
	#undef _LWS_TABLE_HASH_OFFSET_BASIS
	#undef _LWS_TABLE_HASH_PRIME
	return hash;
}

static size_t lws_table_size (lws_table_t *t, size_t size) {
	int  lower, upper, mid;

	lower = 0;
	upper = lws_table_sizes_n - 1;
	while (lower <= upper) {
		mid = (lower + upper) / 2;
		if (lws_table_sizes[mid] < size) {
			lower = mid + 1;
		} else {
			upper = mid - 1;
		}
	}
	if (lower >= lws_table_sizes_n) {
		ngx_log_error(NGX_LOG_ERR, t->log, 0, "[LWS] table size too large: %z", size);
		return (size_t)-1;
	}
	return lws_table_sizes[lower];
}

static size_t lws_table_load (lws_table_t *t, size_t alloc) {
	return (alloc >> 1) + (alloc >> 2) + (alloc >> 3);  /* ~87.5 percent */
}

static int lws_table_rehash (lws_table_t *t, size_t alloc) {
	size_t              alloc_new;
	lws_table_entry_t  *entries, *entries_new, *entry, *entry_new;
	ngx_queue_t        *q, *s;

	/* allocate */
	if ((alloc_new = lws_table_size(t, alloc)) == (size_t)-1) {
		return -1;
	}
	ngx_log_debug3(NGX_LOG_DEBUG_HTTP, t->log, 0, "[LWS] table rehash t:%p old:%z new:%z", t,
			t->alloc, alloc_new);
	entries_new = ngx_calloc(alloc_new * sizeof(lws_table_entry_t), t->log);
	if (!entries_new) {
		return -1;
	}

	/* update table */
	t->alloc = alloc_new;
	t->load = lws_table_load(t, t->alloc);
	entries = t->entries;
	t->entries = entries_new;
	q = ngx_queue_last(&t->order);
	s = ngx_queue_sentinel(&t->order);
	ngx_queue_init(&t->order);

	/* reinsert */
	while (q != s) {
		entry = ngx_queue_data(q, lws_table_entry_t, order);
		entry_new = lws_table_insert(t, &entry->key, entry->hash);
		*entry_new = *entry;
		ngx_queue_insert_head(&t->order, &entry_new->order);
		q = ngx_queue_prev(q);
	}
	ngx_free(entries);
	return 0;
}

static lws_table_entry_t *lws_table_find (lws_table_t *t, ngx_str_t *key, ngx_uint_t hash) {
	size_t              r, h, q;
	lws_table_entry_t  *entry;

	/* Brent's method; source: https://maths-people.anu.edu.au/~brent/pd/rpb013.pdf */
	r = h = hash % t->alloc;
	q = hash % (t->alloc - 2) + 1;
	entry = &t->entries[h];
	while (entry->state != LWS_TES_UNUSED) {
		if (entry->hash == hash && entry->key.len == key->len && (t->ci
				? ngx_strncasecmp(entry->key.data, key->data, key->len)
				: ngx_strncmp(entry->key.data, key->data, key->len)) == 0) {
			return entry;
		}
		h = (h + q) % t->alloc;
		if (h == r) {
			break;
		}
		entry = &t->entries[h];
	}
	return NULL;
}

static lws_table_entry_t *lws_table_insert (lws_table_t *t, ngx_str_t *key, ngx_uint_t hash) {
	size_t              h, q, len_worst, len, q_move, h_move, len_move, len_entry_move;
	lws_table_entry_t  *entry, *entry_move, *entry_move_old, *entry_move_new;

	/* Brent's method; source: https://maths-people.anu.edu.au/~brent/pd/rpb013.pdf */
	/* determine worst case */
	h = hash % t->alloc;
	q = hash % (t->alloc - 2) + 1;
	entry = &t->entries[h];
	len_worst = 1;
	while (entry->state == LWS_TES_SET) {
		h = (h + q) % t->alloc;
		entry = &t->entries[h];
		len_worst++;
	}
	if (len_worst <= 2) {
		/* "worst" case is optimal overall */
		return entry;
	}

	/* check for a better overall outcome by moving conflicting entries */
	entry_move_old = entry_move_new = NULL;
	len_entry_move = (size_t)-1;
	h = hash % t->alloc;
	entry = &t->entries[h];
	len = 1;
	do {
		q_move = entry->hash % (t->alloc - 2) + 1;
		h_move = (h + q_move) % t->alloc;
		entry_move = &t->entries[h_move];
		len_move = 1;
		while (1) {
			if (entry_move->state != LWS_TES_SET) {
				/* we found a better overall outcome */
				entry_move_old = entry;
				entry_move_new = entry_move;
				len_entry_move = len_move;
				break;
			}
			if (len + len_move >= len_worst - 1) {  /* zero-sum at best otherwise */
				break;
			}
			h_move = (h_move + q_move) % t->alloc;
			entry_move = &t->entries[h_move];
			len_move++;
		}
		h = (h + q) % t->alloc;
		entry = &t->entries[h];
		len++;
	} while (len < len_worst - 1 && len < len_entry_move);  /* zero-sum at best otherwise */

	/* move if a better overall outcome was found */
	if (entry_move_old) {  /* implied if len < len_entry_move is false */
		*entry_move_new = *entry_move_old;
		entry_move_new->order.prev->next = &entry_move_new->order;
		entry_move_new->order.next->prev = &entry_move_new->order;
		return entry_move_old;
	}  /* len is invariably len_worst - 1 at this point */

	/* cannot do better than the worst case */
	h = (h + q) % t->alloc;
	entry = &t->entries[h];
	return entry;
}

static void lws_table_remove (lws_table_t *t, lws_table_entry_t *entry) {
	ngx_queue_remove(&entry->order);
	if (t->dup) {
		ngx_free(entry->key.data);
	}
	if (t->free) {
		ngx_free(entry->value);
	}
	entry->state = LWS_TES_DELETED;
	t->count--;
}
