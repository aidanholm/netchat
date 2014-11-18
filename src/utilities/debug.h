/* Simple debugging
 *
 * - coloured logging
 * - pretty assertions
 * - struct type assertions
 * - condition checking macros
 */
#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>
#include <errno.h>
#include <string.h>

#define COL_BLACK   "\x1b[30m"
#define COL_RED     "\x1b[31m"
#define COL_GREEN   "\x1b[32m"
#define COL_YELLOW  "\x1b[33m"
#define COL_BLUE    "\x1b[34m"
#define COL_MAGENTA "\x1b[35m"
#define COL_CYAN    "\x1b[36m"
#define COL_WHITE   "\x1b[37;1m"
#define COL_GREY    "\x1b[37m"
#define COL_RST   "\x1b[0m"

#define COL_CODE COL_BLUE

#define log_lvl(COLOUR, LVL, M, ...) fprintf(stderr, COLOUR LVL COL_RST M \
		COL_GREY " \t(%s:%d) " COL_RST "\n", ##__VA_ARGS__, __FILE__, __LINE__)

#define PRNT_KEY  "'"COL_BLUE"%s"COL_RST"'"
#define PRNT_FILE "'"COL_CYAN"%s"COL_RST"'"

#ifdef NDEBUG /* Debug mode disabled */

#define dbg_struct
#define dbg_struct_init(PTR, NAME)
#define assert_is_struct(PTR, NAME)
#define debug(M, ...)
#define dbg_assert(C, M, ...)

#else /* Debug mode enabled */

#define debug(M, ...)    log_lvl(COL_MAGENTA, "Debug:  ", M, ##__VA_ARGS__)

#define dbg_assert(C, M, ...) do { \
	if(!(C)) { \
		log_lvl(COL_RED, "Assert: ", M, ##__VA_ARGS__);\
		abort(); \
	} \
} while(0)

/*
 * Pthread helper
 */

#define mutex_locked(lockptr) (pthread_mutex_trylock((pthread_mutex_t*)lockptr) == EBUSY)

/*
 *  Structure type debugging.
 */

#define dbg_struct struct {const char *name;} _dbg_struct
typedef dbg_struct;

#define assert_is_struct(PTR, NAME) do {\
		assert(offsetof(NAME, _dbg_struct) == 0); \
		dbg_assert(_is_struct(#NAME, PTR),\
		"wrong struct type: need '" COL_GREEN "%s" COL_WHITE\
		"', not '" COL_GREEN "%s" COL_WHITE "'", #NAME,\
		(PTR) ? ((_dbg_struct*)(PTR))->name : "NULL"); \
	} while(0)

#define dbg_struct_init(PTR, NAME) do {\
	assert(offsetof(NAME, _dbg_struct) == 0); \
	_dbg_struct_init(PTR, #NAME); \
} while(0)

static inline int _is_struct(const char *name, const void *ptr) {
	assert(name);
	if(!ptr) return 0;
	const _dbg_struct *sdbg = ptr;
	if(!sdbg->name) return 0;
	return !strcmp(name, sdbg->name);
}

static inline void _dbg_struct_init(void *ptr, const char *name) {
	assert(ptr);
	assert(name);
	_dbg_struct *sdbg = ptr;
	sdbg->name = name;
}

#endif

/*
 *  Logging functions.
 */

#define log_err(M, ...)  log_lvl(COL_RED,    "Error:  ", M, ##__VA_ARGS__)
#define log_warn(M, ...) log_lvl(COL_YELLOW, "Warning:", M, ##__VA_ARGS__)
#define log_info(M, ...) log_lvl(COL_GREEN,  "Info:   ", M, ##__VA_ARGS__)
#define func_fail() log_err("%s() failed.", __func__)

/*
 *  Checking conditions: they mostly goto error; on failure.
 */

#define check(A, M, ...)\
	{ if(!(A)) { log_err(M, ##__VA_ARGS__); errno=0; goto error; } }
#define check_goto(A, G, M, ...)\
	{ if(!(A)) { log_err(M, ##__VA_ARGS__); errno=0; goto G; } }
#define check_warn(A, M, ...)\
	{ if(!(A)) { log_warn(M, ##__VA_ARGS__); errno=0; } }
#define check_warn_goto(A, G, M, ...)\
	{ if(!(A)) { log_warn(M, ##__VA_ARGS__); errno=0; goto G; } }
#define check_quiet(A)\
	{ if(!(A)) {errno=0; goto error; } }
#define check_debug(A, M, ...)\
	{ if(!(A)) { debug(M, ##__VA_ARGS__); errno=0; goto error; } }
#define check_arg(A, arg, M, ...)\
	check(A, "Bad argument '%s' to %s(): " M, #arg, __func__, ##__VA_ARGS__)
#define check_mem(A)\
	check((A), "Out of memory.")
#define sentinel(M, ...)  do { log_err(M, ##__VA_ARGS__); errno=0; goto error; } while(0)

#endif /* end of include guard: DEBUG_H */
