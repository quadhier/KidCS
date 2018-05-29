#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "kid_map.h"

int equals(const void *key1, const void *key2)
{
	char *strkey1 = (char *)key1; 
	char *strkey2 = (char *)key2;
	if(strcmp(strkey1, strkey2) == 0)
	{
		return 1;
	}
	return 0;
}

int main()
{
	struct kid_map map;
	kid_map_init(&map, 10, equals, free, free);
	char k1[] = "key1";
	char v1[] = "value1";
	kid_map_put(&map, k1, v1);
	char k2[] = "key2";
	char v2[] = "value2";
	if(kid_map_find(&map, k1) != -1)
	{
		printf("found\n");
	}
	else
	{
		printf("not found\n");
	}
	if(kid_map_find(&map, k2) != -1)
	{
		printf("found\n");
	}
	else
	{
		printf("not found\n");
	}

}
