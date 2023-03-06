#pragma once

#define RULENG_ERR(fmt, ...)						\
	do {											\
		fprintf(stderr, "ERROR: %s(): " #fmt "\n",	\
				__func__, ##__VA_ARGS__);			\
	} while(0)

#define RULENG_INFO(fmt, ...)						\
	do {											\
		fprintf(stdout, "INFO: %s(): " #fmt "\n",	\
				__func__, ##__VA_ARGS__);			\
	} while(0)

#define RULENG_DEBUG(fmt, ...)						\
	do {											\
		fprintf(stdout, "DEBUG: %s(): " #fmt "\n",	\
				__func__, ##__VA_ARGS__);			\
	} while(0)
