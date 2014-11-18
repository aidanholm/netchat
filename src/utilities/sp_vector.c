#include <assert.h>
#include <string.h>
#include <alloca.h>
#include "sp_vector.h"
#include "debug.h"
#include "macros.h"

void sp_vector_init(sp_vector_t *sp_vec, size_t item_size) {
	assert(sp_vec);
	assert(item_size > 0);

	sp_vec->_next_free = 0;
	sp_vec->size = 0;
	sp_vec->smallest_id = (size_t)-1;
	sp_vec->largest_id = 0;
	vector_init(&sp_vec->_slots, 1 + max(item_size, sizeof(size_t)));
}

int sp_vector_dup(sp_vector_t *new, const sp_vector_t *old) {
	assert(old);
	assert(new);

	memcpy(new, old, sizeof(*old));
	check_quiet(!vector_dup(&new->_slots, &old->_slots));

	return 0;
error:
	func_fail();
	return 1;
}

void sp_vector_free(sp_vector_t *sp_vec) {
	assert(sp_vec);
	
	vector_free(&sp_vec->_slots);
}

size_t sp_vector_add(sp_vector_t *sp_vec, const void *new_item) {
	assert(sp_vec);
	assert(new_item);

	size_t new_id;
	
	if(sp_vec->_next_free) {
		size_t slot_i = sp_vec->_next_free;
		char *free_flag = vector_get(&sp_vec->_slots, slot_i - 1);
		void *item = free_flag + 1;
		assert(*free_flag); /* This slot's free flag must be on */
		sp_vec->_next_free = *(size_t*)(item); /* Pull out next free index */

		*free_flag = 0;
		memcpy(item, new_item, sp_vec->_slots.item_size - 1);

		new_id = slot_i;
	} else {
		/* Construct an entry with a free flag of zero */
		char *buf = alloca(sp_vec->_slots.item_size);
		buf[0] = 0;
		memcpy(&buf[1], new_item, sp_vec->_slots.item_size - 1);

		check_quiet(vector_add(&sp_vec->_slots, buf, 1));

		new_id = sp_vec->_slots.size;
	}
	sp_vec->size++;

	sp_vec->smallest_id = min(new_id, sp_vec->smallest_id);
	sp_vec->largest_id  = max(new_id, sp_vec->largest_id);

	assert(new_id);
	return new_id;
error:
	log_err("Couldn't add item to sparse vector.");
	assert(0);
	return 0;
}

void *sp_vector_get(const sp_vector_t *sp_vec, size_t i) {
	assert(sp_vec);
	assert(i);

	char *free_flag = vector_get(&sp_vec->_slots, i-1);
	check_quiet(*free_flag == 0);
	void *item = free_flag + 1;

	return item;
error:
	return NULL;
}

int sp_vector_del(sp_vector_t *sp_vec, size_t i) {
	assert(sp_vec);
	assert(i);

	void *item;
	
	check_quiet(item = sp_vector_get(sp_vec, i));

	/* Set the free flag, one byte before the item */
	*((char*)item - 1) = 1;

	/* Add the slot to the linked list of free slots */
	*(size_t*)item = sp_vec->_next_free;
	sp_vec->_next_free = i;

	sp_vec->size--;

	return 0;
error:
	return 1;
}
