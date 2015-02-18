/*
 * $Id: main.c,v 1.5 2015/02/18 23:42:39 urs Exp $
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "lzw.h"

static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s [-dv] in out\n", name);
}

#define TABSIZE (8192 - 2)

#define DEC_WIDTH 0
#define INC_WIDTH 1

static int compress(const char *in, const char *out);
static int decompress(const char *in, const char *out);
static void send(int cd, int bit_width, FILE *fp);
static int receive(int bit_width, FILE *fp);

static int verbose = 0;

int main(int argc, char **argv)
{
	int decompress_flag = 0, errflag = 0;
	int opt;

	while ((opt = getopt(argc, argv, "dv")) != -1) {
		switch (opt) {
		case 'd':
			decompress_flag = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			errflag = 1;
			break;
		}
	}

	if (errflag || argc - optind != 2) {
		usage(argv[0]);
		exit(1);
	}

	if (decompress_flag)
		decompress(argv[optind], argv[optind + 1]);
	else
		compress(argv[optind], argv[optind + 1]);

	return 0;
}

static int compress(const char *in, const char *out)
{
	struct encoder *e;
	int flag;
	int c, cd;
	int bit_width, next_width;
	long nbits = 0, nbytes = 0;
	FILE *infile, *outfile;

	if (!(infile = fopen(in, "rb")))
		return -1;
	if (!(outfile = fopen(out, "wb")))
		return -1;

	e = encode_init(TABSIZE);
	bit_width = 8;
	next_width = 256;
	do {
		c = fgetc(infile);
		nbytes++;
		if (verbose) {
			if (c == EOF)
				printf("EOF : ");
			else if (isprint(c))
				printf("'%c' : ", c);
			else if (c == '\n')
				printf("'\\n': ");
			else if (c == '\r')
				printf("'\\r': ");
			else if (c == '\t')
				printf("'\\t': ");
			else
				printf("0x%.2x: ", c);
		}
		if (flag = encode(e, c, &cd)) {
			cd += 2;
			while (cd >= next_width) {
				send(INC_WIDTH, bit_width, outfile);
				nbits += bit_width;
				bit_width++;
				next_width *= 2;
			}
			send(cd, bit_width, outfile);
			nbits += bit_width;
			if (flag == 2)
				while (bit_width > 8) {
					send(DEC_WIDTH, bit_width, outfile);
					nbits += bit_width;
					bit_width--;
					next_width /= 2;
				}
		}
		if (verbose)
			printf("\n");
	} while (c != EOF);
	nbytes--;
	if (verbose)
		printf("FLSH: ");
	send(-1, 0, outfile);

	encode_free(e);

	if (verbose)
		printf("\n%ld bytes -> %ld = 8 * %ld + %ld bits\n",
		       nbytes, nbits, nbits / 8, nbits % 8);

	fclose(infile);
	fclose(outfile);

	return 0;
}

static int decompress(const char *in, const char *out)
{
	struct decoder *d;
	int cd;
	int bit_width, len;
	unsigned char *buf;
	FILE *infile, *outfile;

	if (!(infile = fopen(in, "rb")))
		return -1;
	if (!(outfile = fopen(out, "wb")))
		return -1;

	d = decode_init(TABSIZE);
	bit_width = 8;
	do {
		if ((cd = receive(bit_width, infile)) < 0)
			break;
		if (verbose)
			printf("(%.4x,%d):\t", cd, bit_width);
		if (cd == DEC_WIDTH) {
			if (verbose)
				printf("DEC\n");
			bit_width--;
		} else if (cd == INC_WIDTH) {
			if (verbose)
				printf("INC\n");
			bit_width++;
		} else {
			cd -= 2;
			len = decode(d, cd, &buf);
			fwrite(buf, 1, len, outfile);
			if (verbose) {
				int i;
				for (i = 0; i < len; i++)
					printf(" %.2x", buf[i]);
				putchar('\n');
			}
		}
	} while (bit_width > 0);

	decode_free(d);

	fclose(infile);
	fclose(outfile);

	return 0;
}

typedef unsigned int WORD;
#define WSIZE (8 * sizeof(WORD))

static void send(int cd, int bit_width, FILE *fp)
{
	static int bits_free = WSIZE;
	static WORD buf = 0;

	/* call send() with cd == -1 to flush the buffer */

	if (cd < 0) {
		cd = 0;
		bit_width = bits_free;
	}

	if (verbose)
		printf(" (%.4x,%d)", cd, bit_width);

	if (bits_free > bit_width) {
		buf |= cd << (bits_free - bit_width);
		bits_free -= bit_width;
	} else {
		buf |= cd >> (bit_width - bits_free);
		fwrite(&buf, sizeof(buf), 1, fp);
		buf = bits_free == bit_width ?
			0 : cd << (WSIZE - (bit_width - bits_free));
		bits_free = WSIZE - (bit_width - bits_free);
	}
}

static int receive(int bit_width, FILE *fp)
{
	static int bits_used = 0;
	static WORD buf;
	int cd;

	if (bits_used >= bit_width) {
		cd = buf >> (bits_used - bit_width);
		bits_used -= bit_width;
	} else {
		cd = buf << (bit_width - bits_used);
		if (fread(&buf, sizeof(buf), 1, fp) != 1)
			return -1;
		cd |= buf >> (WSIZE - (bit_width - bits_used));
		bits_used = WSIZE - (bit_width - bits_used);
	}
	return cd & ((1 << bit_width) - 1);
}
