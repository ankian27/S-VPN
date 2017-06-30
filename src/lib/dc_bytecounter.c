#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <math.h>
//#include "datacomp.h"
#include "dc_bytecounter.h"


#define SLOTS 256
#define BCSIZE 512

//const float thresholds[16] = {0.052412, 0.05539, 0.056949, 0.058168, 0.059187, 0.060164, 0.061006, 0.061894, 0.062803, 0.063701, 0.064651, 0.065696, 0.066776, 0.068069, 0.069872, 0.073275};

unsigned Qtype(unsigned type)
{
	int ldebug = 0;
    unsigned new = 0; 

//	if (dc_one_sid_to_rule_them_all) {
//		return 0;
//	}

    /* three types */
    if (type < 33) {
        new = 3;
    } else if (type < 66) {
		new = 2;
	} else if (type < 100) {
		new = 1;
	} else {
		new = 0;
	}

    /* else it remains 0 -- uncompressible */

    if (ldebug) fprintf(stderr, "Qtype: was: %u now: %u\n", type, new);
    
    return new; 
}


inline int intcmp(const void *aa, const void *bb)
{
	    const int *a = aa, *b = bb;
	    return (*a < *b) ? -1 : (*a > *b);
}

inline int floatcmp(const void *aa, const void *bb)
{
	    const float *a = aa, *b = bb;
	    return (*a < *b) ? -1 : (*a > *b);
}

inline unsigned int bytecounter(char *buf, unsigned int len)
{
	/* count the appearances of each unique byte between
	 * buf and buf + len. 
	 * then, Normalize the values (divide each by len).
	 * 
	 * if the value in the bucket is larger than threshold, add one to
	 * the count of unique "present" bytes.
	 *
	 * return the total number of "present bytes"
	 */

	unsigned counts[SLOTS] = {0};
	unsigned int index = 0;
	unsigned int full = 0;
	const unsigned threshold = len/SLOTS;
	uint8_t *buf8 = (uint8_t *)buf;
	unsigned *count;
	unsigned quit_max = 0;

	for (index = 0; index < len; index++) {

		count = counts + *(buf8 + index);

		if (*count < threshold) {
			
			*count += 1;

			if (*count >= threshold) {
				full++;

				/* we may want to quit out with some answer if the value
				 * reaches a certain level (e.g., 100). If quit_max is set
				 * to zero, we'll optimize this out and not do it.
				 */
				if (quit_max && full >= quit_max) {
					return full;
				}
			}
		}
	}

	return full;

}

inline unsigned int average_unsigned_array(unsigned int *bcounts, unsigned int num_bcounts)
{
	unsigned acc = 0;
	unsigned idx = 0;

	for (idx = 0; idx < num_bcounts; idx++)
	{
		acc = acc + bcounts[idx];
	}
	return (unsigned int)round((1.0 * acc / num_bcounts));
}

unsigned int quantized_bytecounter2(char *buf, unsigned int len, unsigned int min_sample_size)
{

	/* based on the given 'len' and the 'min_sample_size', 
	 * how many chunks are we going to bytecount? */


	unsigned skip_factor = 4;

	unsigned int num_countslots = 0;

	if (len > min_sample_size) {

		num_countslots = len / min_sample_size;
		
	} else {

		/* the sample size is too large for this file. just
		 * sample the whole thing. */
		num_countslots = 1;

	}

	int numchunk = 0;
	unsigned counted = 0;
	unsigned int cursor = 0;
	unsigned int sample_size;
	unsigned average = 0;
	unsigned int bcount_acc = 0; /* accumulator for bytecounting */

	for (numchunk = 0; numchunk < num_countslots; numchunk++) 
	{

		/* we want to skip many of the possible slots to help speed up the
		 * bytecounting operation */
		if (numchunk == 0 || (numchunk % skip_factor == 0)) {

			/* we don't want to risk skewing results by bytecounting
			 * chunks smaller than min_sample size. SO, if the *next* chunk
			 * would be smaller than min_sample_size, just bytecount all
			 * remaining data. (otherwise just do min_sample_size.
			 */

			if (cursor + (min_sample_size * 2) > len) 
			{
				sample_size = len - cursor;
			
			} else {
			
				sample_size = min_sample_size;
			
			}

			/* store bytecount value into accumulator */
			bcount_acc += bytecounter((char *)(buf + cursor), sample_size);
			counted++;
		}

		/* regardless of whether we did a bytecount, increment cursor 
		 * to new position into buffer. in this way, we skip over the non-counted buffers */
		cursor = cursor + sample_size;
	}

	/* average the values in the bcount_acc */
	average = bcount_acc / counted;

	return average;

}


unsigned int quantized_bytecounter(char *buf, unsigned int len, unsigned int min_sample_size)
{

	/* based on the given 'len' and the 'min_sample_size', 
	 * how many chunks are we going to bytecount? */

	unsigned int num_bcounts = 0;

	if (len > min_sample_size) {

		num_bcounts = len / min_sample_size;
		
	} else {

		/* the sample size is too large for this file. just
		 * sample the whole thing. */
		num_bcounts = 1;

	}

	int numchunk = 0;
	unsigned int cursor = 0;
	unsigned int sample_size;
	unsigned average = 0;
	unsigned int bcount_acc = 0; /* accumulator for bytecounting */

	for (numchunk = 0; numchunk < num_bcounts; numchunk++) 
	{
		/* we don't want to risk skewing results by bytecounting
		 * chunks smaller than min_sample size. SO, if the *next* chunk
		 * would be smaller than min_sample_size, just bytecount all
		 * remaining data. (otherwise just do min_sample_size.
		 */
		if (cursor + (min_sample_size * 2) > len) 
		{
			sample_size = len - cursor;
		
		} else {
		
			sample_size = min_sample_size;
		
		}

		/* store bytecount value into accumulator */
		bcount_acc += bytecounter((char *)(buf + cursor), sample_size);

		/* increment cursor to new position into buffer */
		cursor = cursor + sample_size;
	}

	/* average the values in the bcount_acc */
	average = bcount_acc / num_bcounts;

	return average;

}

unsigned int striding_bytecounter(char *buf, unsigned int len, unsigned int fraction)
{

	/* based on the given 'len' and the 'min_sample_size', 
	 * how many chunks are we going to bytecount? */

	unsigned int num_bcounts = 0;
	unsigned int inter = 0;
	int ldebug = 0;

	if (len > (fraction * BCSIZE)) {

		num_bcounts = (len / (fraction * BCSIZE)); /* not sure why this fencepost */
		
	} else {

		/* the sample size is too large for this file. just
		 * sample the whole thing. */
		num_bcounts = 1;

	}

	inter = (len - (num_bcounts * BCSIZE)) / num_bcounts;

	if (ldebug) { 
		fprintf(stderr, "stby: len: %u frac: %u nbc: %u inter: %u\n",
			len, fraction, num_bcounts, inter); 
	}

	/* create an array to hold the results of each sample */
	int numchunk = 0;
	unsigned int cursor = 0;
	unsigned average = 0;
	unsigned int bcount_acc = 0;

	for (numchunk = 0; numchunk < num_bcounts; numchunk++) 
	{

		/* store bytecount value into array */
		bcount_acc += bytecounter((char *)(buf + cursor), BCSIZE);

		/* increment cursor to new position */
		cursor = cursor + BCSIZE + inter;
		if (ldebug) {
			fprintf(stderr, "numchunk: %d bcacc: %u cursor: %u\n",
				numchunk, bcount_acc, cursor);
		}
	}

	/* average the values in bcounts */
	average = bcount_acc / num_bcounts;
	if (ldebug) {
		fprintf(stderr, "bcacc: %u num_bcounts: %u avg: %u\n", bcount_acc, num_bcounts, average);
	}

	return average;

}

