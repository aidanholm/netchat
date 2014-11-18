#ifndef STATUS_H
#define STATUS_H

typedef enum _user_status_t {
	US_invalid = -1,
	US_ONLINE,
	US_AWAY,
	US_BUSY,
	US_OFFLINE,
	US_NUM_STATUSES,
	US_IDENTIFY,
} user_status_t;

#endif
