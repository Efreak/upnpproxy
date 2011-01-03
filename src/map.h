#ifndef MAP_H
#define MAP_H

typedef struct _map_t* map_t;
typedef uint32_t (* map_hash_t)(const void* element);
typedef bool (* map_eq_t)(const void* e1, const void* e2);
typedef void (* map_free_t)(void* element);

/* Only free_func may be NULL, if it is, it is not called */
map_t map_new(size_t elementsize, map_hash_t hash_func, map_eq_t eq_func,
              map_free_t free_func);

void map_free(map_t map);

size_t map_size(map_t map);

/* The data in element is copied */
void* map_put(map_t map, const void* element);

/* Returns the element in the map that returns true for eq_func */
void* map_get(map_t map, const void* element);

/* Removes all elements in the map that returns true for eq_func */
size_t map_remove(map_t map, const void* element);

void* map_getat(map_t map, size_t idx);
size_t map_begin(map_t map);
size_t map_end(map_t map);
size_t map_next(map_t map, size_t idx);
size_t map_removeat(map_t map, size_t idx);

#endif /* MAP_H */
