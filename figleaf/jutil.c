/*
 * Copyright 2002 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/types.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <err.h>
#include <math.h>

#include <jpeglib.h>
#include <jutil.h>

/* Prints a single block */

void
print_block(short *block)
{
	int j;

	for (j = 0; j < DCTSIZE2; j++) {
		if (j && (j % DCTSIZE) == 0)
			printf("\n");
		printf("%3d ", block[j]);
	}
	printf("\n");
}




/* Dequantize block */

void
dequant_block(short *out, short *in, JQUANT_TBL *table)
{
	int j;

	for (j = 0; j < DCTSIZE2; j++) {
		out[j] = in[j] * table->quantval[j];
	}
}

/* Quantize block */

void
quant_block(short *out, short *in, JQUANT_TBL *table)
{
	int j;

	for (j = 0; j < DCTSIZE2; j++) {
		out[j] = in[j] / table->quantval[j];
	}
}

int
count_edge(short *in)
{
	int i, sum;

	sum = 0;

	for (i = 0; i < DCTSIZE; i++) {
		sum += abs(in[i]);
		sum += abs(in[(DCTSIZE-1) * DCTSIZE + i]);

		if (i == 0 || i == DCTSIZE - 1)
			continue;

		sum += abs(in[i * DCTSIZE]);
		sum += abs(in[i * DCTSIZE + DCTSIZE - 1]);
	}

	return (sum);
}

int
count_all(short *in)
{
	int i, sum;

	sum = 0;

	for (i = 0; i < DCTSIZE2; i++) {
		sum += abs(in[i]);
	}

	return (sum);
}

struct jeasy *
jpeg_prepare_blocks(struct jpeg_decompress_struct *jsrc)
{
	jvirt_barray_ptr *dctcoeff = jpeg_read_coefficients(jsrc);
	struct jeasy *je;
	int i, j;

	if ((je = malloc(sizeof(struct jeasy))) == NULL)
		err(1, "malloc");

	memset(je, 0, sizeof(struct jeasy));
	je->jinfo = jsrc;
	je->blocks = malloc(jsrc->num_components * sizeof(short **));
	if (je->blocks == NULL)
		err(1, "malloc");

	je->comp = jsrc->num_components;
	/* Read first ten rows of first component */
	for (i = 0; i < jsrc->num_components; i++) {
		JBLOCKARRAY rows;
		int wib = jsrc->comp_info[i].width_in_blocks;
		int hib = jsrc->comp_info[i].height_in_blocks;

		je->table[i] = jsrc->comp_info[i].quant_table;
		je->height[i] = hib;
		je->width[i] = wib;

		je->blocks[i] = malloc(wib * hib * sizeof(short *));
		if (je->blocks[i] == NULL)
			err(1, "malloc");

		for (j = 0; j < wib * hib; j++) {
			je->blocks[i][j] = malloc(DCTSIZE2 * sizeof(short));
			if (je->blocks[i][j] == NULL)
				err(1, "malloc");
		}

		for (j = 0; j < hib; j++) {
			JBLOCKROW row;
			int k, l;

			rows = jsrc->mem->access_virt_barray((j_common_ptr)&jsrc, dctcoeff[i], j, 1, 1);
			if (rows == NULL)
				errx(1, "Access failed");

			row = rows[0];

			for (k = 0; k < wib; k++)
				for (l = 0; l < DCTSIZE2; l++)
					je->blocks[i][j * wib + k][l] = row[k][l];
		}
	}
	return (je);
}

void
jpeg_return_blocks(struct jeasy *je, struct jpeg_decompress_struct *jsrc)
{
	jvirt_barray_ptr *dctcoeff = jpeg_read_coefficients(jsrc);
	short ***blocks = je->blocks;
	int i, j;

	/* Read first ten rows of first component */
	for (i = 0; i < jsrc->num_components; i++) {
		JBLOCKARRAY rows;
		int wib = jsrc->comp_info[i].width_in_blocks;
		int hib = jsrc->comp_info[i].height_in_blocks;

		for (j = 0; j < hib; j++) {
			JBLOCKROW row;
			int k, l;

			rows = jsrc->mem->access_virt_barray((j_common_ptr)&jsrc, dctcoeff[i], j, 1, 1);
			if (rows == NULL)
				errx(1, "Access failed");

			row = rows[0];

			for (k = 0; k < wib; k++)
				for (l = 0; l < DCTSIZE2; l++)
					row[k][l] = blocks[i][j * wib + k][l];
		}
	}

}

void
jpeg_free_blocks(struct jeasy *je)
{
	int i, j;
	short ***blocks = je->blocks;

	/* Read first ten rows of first component */
	for (i = 0; i < je->comp; i++) {
		int wib = je->width[i];
		int hib = je->height[i];

		for (j = 0; j < hib; j++) {
			int k;

			for (k = 0; k < wib; k++)
				free (blocks[i][j * wib + k]);
		}
		free(blocks[i]);
	}
	free(blocks);
	free(je);
}

int
diff_horizontal(short *left, short *right)
{
	int i, sum;

	sum = 0;
	for (i = 0; i < DCTSIZE; i++)
		sum += abs(left[i*DCTSIZE + DCTSIZE - 1] - right[i*DCTSIZE]);

	return (sum);
}

int
diff_vertical(short *up, short *down)
{
	int i, sum;

	sum = 0;
	for (i = 0; i < DCTSIZE; i++)
		sum += abs(up[(DCTSIZE-1)*DCTSIZE+i] - down[i]);

	return (sum);
}

int
iterate_all(struct jeasy *je,
    int (*f)(struct jeasy *, int, int, int, void *), void *arg)
{
	int i, j, k;

	for (i = 0; i < je->comp; i++)
		for (j = 0; j < je->height[i]; j++)
			for (k = 0; k < je->width[i]; k++)
				if (f(je, i, j, k, arg) == -1)
					return (-1);

	return (0);
}



double
variance(short *block)
{
	double mean, tmp;
	int n;

	mean = 0;
	for (n = 0; n < DCTSIZE2; n++)
		mean += block[n];
	mean /= DCTSIZE2;

	tmp = 0;
	for (n = 0; n < DCTSIZE2; n++)
		tmp += (block[n] - mean)*(block[n] - mean);

	return (sqrt(tmp / (DCTSIZE2-1)));
}


