#ifndef MACROS_H
#define MACROS_H

#define max(a,b) ((a)>(b)?(a):(b))
#define min(a,b) ((a)<(b)?(a):(b))

#define sort_by(a,b) (\
	(a) < (b) ? -1 : \
	(a) > (b) ?  1 : 0)

#endif
