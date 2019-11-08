/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "bl_map.h"

#include <string.h> /* strcmp */

/* --- global functions --- */

int bl_map_rehash(int hash_key, u_int size) {
  if (++hash_key >= size) {
    return 0;
  } else {
    return hash_key;
  }
}

int bl_map_hash_str(char *key, u_int size) {
  int hash_key;

  hash_key = 0;

  while (*key) {
    hash_key += *key++;
  }

  return hash_key % size;
}

int bl_map_hash_int(int key, u_int size) { return key % size; }

int bl_map_hash_int_fast(int key, u_int size /* == 2^n */
                         ) {
  return key & (size - 1);
}

int bl_map_compare_str(char *key1, char *key2) { return (strcmp(key1, key2) == 0); }

int bl_map_compare_str_nocase(char *key1, char *key2) { return (strcasecmp(key1, key2) == 0); }

int bl_map_compare_int(int key1, int key2) { return (key1 == key2); }

#ifdef BL_DEBUG

#include <assert.h>

#undef DEFAULT_MAP_SIZE
#define DEFAULT_MAP_SIZE 4
#undef MAP_MARGIN_SIZE
#define MAP_MARGIN_SIZE 2

BL_MAP_TYPEDEF(test, int, char *);

void TEST_bl_map(void) {
  BL_MAP(test) map;
  BL_PAIR(test) pair;
  BL_PAIR(test) * array;
  u_int size;
  int result;
  int key;
  char *table[] = {
      "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k",
  };

  bl_map_new_with_size(int, char *, map, bl_map_hash_int, bl_map_compare_int, DEFAULT_MAP_SIZE);

  for (key = 0; key < sizeof(table) / sizeof(table[0]); key++) {
    bl_map_set(result, map, key * 3, table[key]);
  }

  assert(map->map_size ==
         ((sizeof(table) / sizeof(table[0]) + MAP_MARGIN_SIZE + DEFAULT_MAP_SIZE - 1)
          / DEFAULT_MAP_SIZE) * DEFAULT_MAP_SIZE); /* 16 */
  assert(map->filled_size == sizeof(table) / sizeof(table[0]));

  for (key = 0; key < sizeof(table) / sizeof(table[0]); key++) {
    bl_map_get(map, key * 3, pair);
    assert(strcmp(table[key], pair->value) == 0);
  }

  for (key = 0; key < sizeof(table) / sizeof(table[0]) - 2; key++) {
    bl_map_erase(result, map, key * 3);
  }

  assert(map->map_size == 8);
  assert(map->filled_size == 2);

  for (key = 0; key < sizeof(table) / sizeof(table[0]); key++) {
    bl_map_get(map, key * 3, pair);
    if (key >= sizeof(table) / sizeof(table[0]) - 2) {
      assert(strcmp(table[key], pair->value) == 0);
    } else {
      assert(pair == NULL);
    }
  }

  bl_map_get_pairs_array(map, array, size);

  for (key = 0; key < size; key++) {
    assert(strcmp(table[key + sizeof(table) / sizeof(table[0]) - 2], array[key]->value) == 0);
  }

  bl_map_destroy(map);

  bl_msg_printf("PASS bl_map test.\n");
}

#endif
