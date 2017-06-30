#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <zlib.h>
#include "minicomp.h"

#define Z_STRENGTH 1
#define MCDEBUG 1


/* report a zlib or i/o error */
void zerr(int ret)
{
    fputs("zpipe: ", stderr);
    switch (ret) {
    case Z_ERRNO:
        if (ferror(stdin))
            fputs("error reading stdin\n", stderr);
        if (ferror(stdout))
            fputs("error writing stdout\n", stderr);
        break;
    case Z_STREAM_ERROR:
        fputs("invalid compression level\n", stderr);
        break;
    case Z_DATA_ERROR:
        fputs("invalid or incomplete deflate data\n", stderr);
        break;
    case Z_MEM_ERROR:
        fputs("out of memory\n", stderr);
        break;
    case Z_VERSION_ERROR:
        fputs("zlib version mismatch!\n", stderr);
    }
}

int mcdecompress(void *dest, const void *src, size_t len, size_t destlen)
{

	int ret = 0;
	unsigned free_space = 0;
	z_stream strm;

	if (len == 0) {
		fprintf(stderr, "Requested decompression of length 0. Returning.\n");
		return len;
	}

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;

    /* initialize inbound state */
    if ((ret = inflateInit(&strm)) != Z_OK) {
        perror("first inflateInit failed.");
        abort();
    }    

	free_space = strm.avail_out = destlen;
	strm.next_out = (unsigned char *)dest;

	strm.avail_in = len;
	strm.next_in = (unsigned char *)src;

	/* inflate until the end of the stream */
	ret = inflate(&strm, Z_NO_FLUSH);
	assert(ret != Z_STREAM_ERROR);

	/* check the output state */
	switch(ret) {
		case Z_NEED_DICT:
			ret = Z_DATA_ERROR;
		case Z_DATA_ERROR:
		case Z_MEM_ERROR:
			(void)inflateEnd(&strm);
			fprintf(stderr, "POST: error inflating data: ");
			zerr(ret);
			abort();
	}

	if (ret != Z_STREAM_END) {
		fprintf(stderr, "ret != Z_STREAM_END!\n");
		abort();
	}

	/* sanity check
	if (task->dcsh.bytes_uncompressed != (free_space - strm.avail_out)) {
		fprintf(stderr, "%u != %u!\n", task->dcsh.bytes_uncompressed,
			   (free_space - strm.avail_out));
		abort();
	}
	*/

	inflateEnd(&strm);

	printf("consumed %lu and produced %u bytes...\n", 
						 len,
						 free_space - strm.avail_out);

	return free_space - strm.avail_out;

}


int mccompress(void *dest, const void *src, size_t len, size_t destlen) {

	/* declare important zlib variables and init them */
	int ret = 0;
	z_stream strm;
	int bytes_compressed = 0;
   	int bytes_processed = 0;

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0; 
    strm.next_in = Z_NULL;
	
	/* initialize the deflate state */
	if ((ret = deflateInit(&strm, Z_STRENGTH)) != Z_OK) {
		perror("calling deflateInit!");
		abort();
	}
	
	/* set up variables */
	strm.avail_in = len;
	strm.next_in = (unsigned char *)src;
	strm.avail_out = destlen;
	strm.next_out = (unsigned char *)dest;

	/* do compression */
	ret = deflate(&strm, Z_FINISH); /* deflate will update the strm members */
	assert(ret != Z_STREAM_ERROR);
	
	/* how many bytes did we compress? */
	bytes_processed = len - strm.avail_in;

	/* determine how many bytes of output are ready */
	bytes_compressed = destlen - strm.avail_out; 

	if (MCDEBUG) fprintf(stderr, "Compressed %lu bytes to %d.\n", 
						 (long unsigned)len, bytes_compressed);

	/* sanity check */
	if (ret != Z_STREAM_END) {

		fprintf(stderr, "ret != Z_STREAM_END?!\n");
		abort();
	}

	/* do this to finish things */
	(void)deflateEnd(&strm);

	return bytes_compressed;

}

unsigned long get_complen(const void *src)
{
	/* returns the length of the compressed data segment */
	struct mcheader *header = (struct mcheader*) src;
	return header->csize;
}


unsigned long get_decomplen(const void *src)
{
	/* returns the number of bytes required to store the data
	 * when uncompressed */
	struct mcheader *header = (struct mcheader*) src;
	return header->ucsize;
}

int is_compressed(const void *src)
{
	/* returns 1 if data is compressed or 0 if not */
	struct mcheader *header = (struct mcheader*) src;
	return header->is_compressed;
}

int is_mcbuffer(const void *src)
{

	/* returns 1 if src has a valid magic number or 0 if not */
	struct mcheader *header = (struct mcheader*) src;
	if (header->magic == DCMAGIC) {
		return 1;
	} else {
		return 0;
	}

}

void print_header(const void *src)
{

	if (!is_mcbuffer(src)) {
		printf("src is not a mcbuffer!\n");
	} else {
		struct mcheader *header = (struct mcheader*) src;
		printf("magic: %lx\n", header->magic);
		printf("is_compressed: %u\n", is_compressed(src));
		printf("csize: %lu\n", get_complen(src));
		printf("ucsize: %lu\n", get_decomplen(src));
	}

}

int minicomp(void *dest, const void *src, size_t len, size_t destlen) {

	int ret;
	struct mcheader *header = (struct mcheader*) dest;
	void *dest_data = dest + sizeof(struct mcheader);
	unsigned int bytecount;

	/* fill header with obvious values */
	header->magic = DCMAGIC;
	header->ucsize = len;

	/* use bytecounting algorithm to determine whether to compress */
	bytecount = bytecounter((char *)src, (unsigned int)len);

	if (Qtype(bytecount) == 0 || len < MINCOMPRESS) {
		if (MCDEBUG) printf("not compressing; bytecount is %d or len (%lu) is < %d\n", 
						  bytecount, len, MINCOMPRESS);
		header->is_compressed = NOTCOMPED; /* we're not compressing */
		header->csize = len; /* compressed size is unchanged */
		memcpy(dest_data, src, len);
		ret = len;

	} else {
		
		if (MCDEBUG) printf("compressing; bytecount is %d\n", bytecount);

		header->is_compressed = COMPED; /* we are compressing! */
		ret = mccompress(dest_data, src, len, destlen); /* do compression */
		header->csize = ret; /* update compressed size */
	}
	
	if (MCDEBUG) print_header(header);	


	ret += sizeof(struct mcheader); /* add the header regardless of compression */

	return ret;

}

int minidecomp(void *dest, const void *src, size_t len, size_t destlen) 
{

	int ret;
	struct mcheader *header = (struct mcheader*) src;
	const void *src_data = src + sizeof(struct mcheader);
	int datalen = len - sizeof(struct mcheader);


	if (MCDEBUG) print_header(header);	

	if (!is_mcbuffer(src)) {
		printf("header doesn't match magic!\n");
		abort();
	}

	if (is_compressed(src)) {
		if (MCDEBUG) printf("input compressed; decompressing...\n");
		ret = mcdecompress(dest, src_data, len, destlen);
	} else {
		if (MCDEBUG) printf("input not compressed! copying...\n");
		memcpy(dest, src_data, datalen);
		ret = datalen;
	}

	return ret;
}


