#ifndef KIDMAP
#define KIDMAP

struct kid_map
{
	int size;
	int capacity;
	int expand_factor;
	void **keys;
	void **values;
	int (* equals)(const void *key1, const void *key2);
	void (* freekey)(void *);
	void (* freeval)(void *);
};

void 
kid_map_init(
	struct kid_map *map, 
	int capacity, 
	int (* equals)(const void *, const void *),
	void (* freekey)(void *),
	void (* freeval)(void *));

void
kid_map_free(struct kid_map *map);

void kid_map_shrink(struct kid_map *map);

int 
kid_map_find(
	const struct kid_map *map, 
	const void *key);

void 
kid_map_put(
	struct kid_map *map, 
	void *key, 
	void *value);

const void * 
kid_map_get(
	const struct kid_map *map, 
	const void *key);

#endif
