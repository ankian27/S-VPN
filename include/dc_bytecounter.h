#ifndef DC_BC1_H
#define DC_BC1_H

#if __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 600
#else
#define _XOPEN_SOURCE 500
#endif /* __STDC_VERSION__ */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>

# define SLOTS 256

unsigned Qtype(unsigned type);

/* does bytecounting between buf and buf + len, returns count */
unsigned int bytecounter(char *buf, unsigned int len);

/* does bytecounting in chunks of min_sample_size and averages */
unsigned int quantized_bytecounter(char *buf, unsigned int len, unsigned int min_sample_size);
unsigned int quantized_bytecounter2(char *buf, unsigned int len, unsigned int min_sample_size);
unsigned int striding_bytecounter(char *buf, unsigned int len, unsigned int fraction);

#endif /* DC_BC1_H */
