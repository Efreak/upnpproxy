/*
 * Copyright (C) 2011, Joel Klinghed.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

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
