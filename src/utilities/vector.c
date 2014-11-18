#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
 
#include "vector.h"
#include "memory.h"
#include "debug.h"
#include "macros.h"

inline void vector_init(vector_t *v, size_t item_size) {
	assert(v);
	assert(item_size > 0);
	memset(v, 0, sizeof(*v));
	v->item_size = item_size;
}

inline int vector_init_alloc(vector_t *v, size_t item_size, size_t num) {
	vector_init(v, item_size);
	check_mem(v->data = malloc(item_size*num));
	v->_alloc_size = num;

	return 0;
error:
	func_fail();
	return 1;
}

int vector_dup(vector_t *new, const vector_t *old) {
	assert(old);
	assert(new);

	size_t size;

	memcpy(new, old, sizeof(*old));

	if((size = new->_alloc_size*new->item_size)) {
		check_mem(new->data = malloc(size));
		memcpy(new->data, old->data, size);
	} else {
		assert(!new->data);
	}

	return 0;
error:
	func_fail();
	return 1;
}

inline void vector_free(vector_t *v) {
	assert(v);
	free(v->data);
	v->data = NULL;
	v->size = v->_alloc_size = 0;
}

inline void *vector_add(vector_t *v, const void *data, size_t num) {
	assert(v);

	void *result;

	if (v->_alloc_size < v->size + num) {
		size_t new_size = (v->size + num)<<1;
		void *nd = realloc(v->data, v->item_size * new_size);
		check_mem(nd);
		v->data = nd;
		v->_alloc_size += num;
	}
	assert(v->data);

	if(data) memcpy(vector_get(v, v->size), data, v->item_size*num);
	result = vector_get(v, v->size);
	v->size += num;
	return result;

error:
	func_fail();
	return NULL;
}

inline int vector_set(vector_t *v, size_t index, const void *data, size_t num) {
	assert(v);
	assert(data);
	assert(num > 0);

	size_t required_size = index + num;

	/* FIXME: Initialize the vector if it isn't already initialized */
	if(v->_alloc_size == 0)
		vector_init_alloc(v, v->item_size, required_size);

	/* If there's not enough space to fit the new elements, expand */
	if(index+num > v->_alloc_size) {
		void *nd;
		check_mem(nd = realloc(v->data, v->item_size*required_size));
		v->data = nd;
		v->_alloc_size = index;
	}

	/* If we're adding new elements on the end, zero the ones in the middle */
	if(index >= v->size) {
		unsigned int d = index - v->size;
		memset(v->data + v->size*v->item_size, 0, v->item_size*d);
	}

	v->size = max(required_size, v->size);
	memcpy(vector_get(v, index), data, v->item_size*num);

	return 0;
error:
	return 1;
}

inline void *vector_get(const vector_t *v, size_t index) {
	assert(v);
	assert(v->data);
	assert(index < v->_alloc_size);
	return (char*)v->data + index*v->item_size;
}
