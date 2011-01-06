#ifndef VECTOR_H
#define VECTOR_H

typedef struct _vector_t* vector_t;

vector_t vector_new(size_t elementsize);
void vector_free(vector_t vector);

size_t vector_size(vector_t vector);

void* vector_get(vector_t vector, size_t idx);
void vector_set(vector_t vector, size_t idx, const void* data);
void vector_insert(vector_t vector, size_t idx, const void* data);
void* vector_add(vector_t vector);

void vector_push(vector_t vector, const void* data);
void* vector_pop(vector_t vector, void* data);

void vector_remove(vector_t vector, size_t idx);
/* Remove [begin...end[ */
void vector_removerange(vector_t vector, size_t begin, size_t end);

#endif /* VECTOR_H */
