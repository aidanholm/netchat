#ifndef VECTOR_H
#define VECTOR_H

#include <assert.h>
#include <stddef.h>

typedef struct _vector_t {
    void* data;
    size_t size, item_size, _alloc_size;
} vector_t;

void vector_init(vector_t *v, size_t item_size);
int vector_init_alloc(vector_t *v, size_t item_size, size_t num);
int vector_dup(vector_t *new, const vector_t *old);
void vector_free(vector_t *v);
void *vector_add(vector_t *v, const void *data, size_t num);
int vector_set(vector_t *v, size_t index, const void *data, size_t num);
void *vector_get(const vector_t *v, size_t index);

static inline size_t vector_indexof(const vector_t *vector, const void *item) {
	assert(vector);
	assert(item);

	ptrdiff_t diff = item - (void*)(vector->data);

	size_t index = (unsigned)diff/vector->item_size;

	assert(index < vector->size);

	return index;
}

/* Fast delete: just swaps with the last element */
static inline void vector_del(vector_t *vector, size_t i) {
	assert(vector);
	assert(i < vector->size);

	if(i != vector->size-1)
		vector_set(vector, i, vector_get(vector, vector->size-1), 1);
	vector->size--;
}

#define vector_foreach(vec, ptr) \
	for(size_t _v_iter=0; _v_iter<(vec)->size && ((ptr) = vector_get((vec),_v_iter)); _v_iter++)

#define vector_filter(vec, cond)\
	do {\
		assert(vec);\
		void *item;\
		vector_foreach((vec), item) {\
			if(!(cond)) { \
				void *_v_last_item = vector_get((vec), (vec)->size-1);\
				if(_v_last_item != item) \
					memcpy(item, _v_last_item, (vec)->item_size);\
				(vec)->size--;\
			}\
		}\
	} while(0)

#endif /* end of include guard: VECTOR_H */
