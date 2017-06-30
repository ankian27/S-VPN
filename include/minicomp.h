#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>
#include "dc_bytecounter.h"

#ifndef MINICOMP_H_
#define MINICOMP_H_

#define DCMAGIC 0xbadab00fbeefb0b0
#define COMPED 1
#define NOTCOMPED 0
#define MINCOMPRESS 128 /* don't compress unless the data is at least N bytes long */

struct mcheader {
	unsigned long magic;
	unsigned is_compressed;
	unsigned long ucsize;
	unsigned long csize;
};

/* function for decoding zlib errors */
void zerr(int ret);

/* compress src of len into dest and return new length */
int minicomp(void *dest, const void *src, size_t len, size_t destlen); 

/* decompress src of len into dest and return new length */
int minidecomp(void *dest, const void *src, size_t len, size_t destlen);

unsigned long get_decomplen(const void *src); /* return the ucsize of this buffer */
unsigned long get_complen(const void *src); /* return the csize of this buffer */
int is_compressed(const void *src); /* determine if this buffer is compressesd */
int is_mcbuffer(const void *src); /* check for a valid magic number */
void print_header(const void *src); /* print the values of the header to stdout */

/* mostly internal functions but you can use these if you want */

int mccompress(void *dest, const void *src, size_t len, size_t destlen); /* do compression */
int mcdecompress(void *dest, const void *src, size_t len, size_t destlen); /* do compression */

#endif /* MINICOMP_H_ */
