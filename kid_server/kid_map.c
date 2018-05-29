#include <stdlib.h>
#include "kid_map.h"

//
// simple key-value map for kid server
// find and insert in O(n)
//
// author: quadhier
//

//struct kid_map
//{
//	int size;
//	int capacity;
//	int expand_factor;
//	void **keys;
//	void **values;
//	int (* equals)(const void *key1, const void *key2);
//};

void 
kid_map_init(
	struct kid_map *map, 
	int capacity, 
	int (* equals)(const void *, const void *),
	void (* freekey)(void *),
	void (* freeval)(void *))
{
	map->size = 0;
	map->capacity = capacity;
	map->expand_factor = 2;
	map->keys = (void **)malloc(capacity * sizeof(void *));
	map->values = (void **)malloc(capacity * sizeof(void *));
	map->equals = equals;
	map->freekey = freekey;
	map->freeval = freeval;

	for(int i = 0; i < capacity; i++)
	{
		map->keys[i] = NULL;
		map->values[i] = NULL;
	}

}

void 
kid_map_free(struct kid_map *map)
{
	for(int i = 0; i < map->size; i++)
	{
		if(map->keys[i] != NULL)
			map->freekey(map->keys[i]);

		if(map->values[i] != NULL)
			map->freeval(map->values[i]);
	}
}

void 
kid_map_expand(struct kid_map *map)
{
	int new_size = map->expand_factor * map->capacity;
	map->keys = (void **)realloc(map->keys, new_size);	
	map->values = (void **)realloc(map->values, new_size);
}

void 
kid_map_shrink(struct kid_map *map)
{
	int new_size = map->size;	
	map->keys = (void **)realloc(map->keys, new_size);
	map->values = (void **)realloc(map->values, new_size);
}

int 
kid_map_find(
	const struct kid_map *map, 
	const void *key)
{
	if(key == NULL)
	{
		return -1;
	}

	int found = -1;
	for(int i = 0; i < map->size; i++)
	{
		if(map->equals(map->keys[i], key))
		{
			found = i;
			break;
		}
	}
	return found;
}

void 
kid_map_put(
	struct kid_map *map, 
	void *key, 
	void *value)
{
	int pos = kid_map_find(map, key);
	if(pos != -1)
	{
		map->freeval(map->values[pos]);
		map->values[pos] = value;
		return;
	}

	if(map->size == map->capacity)
	{
		kid_map_expand(map);
	}
	map->keys[map->size] = key;
	map->values[map->size] = value; 
	map->size++;
}

const void * 
kid_map_get(
	const struct kid_map *map, 
	const void *key)
{
	int pos = kid_map_find(map, key);
	if(pos == -1)		
		return NULL;

	return map->values[pos];
}

