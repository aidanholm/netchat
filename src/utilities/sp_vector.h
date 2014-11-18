#ifndef SP_VECTOR_H
#define SP_VECTOR_H

#include <string.h>
#include "vector.h"

typedef struct _sp_vector_t {
	vector_t _slots;
	size_t _next_free;
	size_t size;
	size_t smallest_id,
		   largest_id;
} sp_vector_t;

void sp_vector_init(sp_vector_t *sp_vec, size_t item_size);
int sp_vector_dup(sp_vector_t *new, const sp_vector_t *old);
void sp_vector_free(sp_vector_t *sp_vec);
size_t sp_vector_add(sp_vector_t *sp_vec, const void *item);
void *sp_vector_get(const sp_vector_t *sp_vec, size_t i);
int sp_vector_del(sp_vector_t *sp_vec, size_t i);

static inline size_t sp_vector_indexof(const sp_vector_t *sp_vec, const void *item) {
	assert(sp_vec);
	assert(item);

	return vector_indexof(&sp_vec->_slots, item)+1;
}

#define _sp_vec_slot_used(slot) (!*(char*)slot)

#define sp_vector_foreach(sp_vec, ptr) \
	vector_foreach(&(sp_vec)->_slots, ptr) \
		if(!(_sp_vec_slot_used(ptr) && (ptr = (void*)((char*)ptr + 1)))) {} else

#endif /* end of include guard: SP_VECTOR_H */

