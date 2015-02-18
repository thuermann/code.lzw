/*
 * $Id: main.c,v 1.7 2015/02/18 23:45:17 urs Exp $
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "lzw.h"

static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s [-dv]\n", name);
}

#define TABSIZE (8192 - 2)

#define DEC_WIDTH 0
#define INC_WIDTH 1

static int compress(FILE *infile, FILE *outfile);
static int decompress(FILE *infile, FILE *outfile);
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

	if (errflag || optind != argc) {
		usage(argv[0]);
		exit(1);
	}

	if (decompress_flag)
		decompress(stdin, stdout);
	else
		compress(stdin, stdout);

	return 0;
}

static int compress(FILE *infile, FILE *outfile)
{
	struct encoder *e;
	int flag;
	int c, cd;
	int bit_width, next_width;
	long nbits = 0, nbytes = 0;

	e = encode_init(TABSIZE);
	bit_width = 8;
	next_width = 256;
	do {
		c = fgetc(infile);
		nbytes++;
		if (verbose) {
			if (c == EOF)
				fprintf(stderr, "EOF : ");
			else if (isprint(c))
				fprintf(stderr, "'%c' : ", c);
			else if (c == '\n')
				fprintf(stderr, "'\\n': ");
			else if (c == '\r')
				fprintf(stderr, "'\\r': ");
			else if (c == '\t')
				fprintf(stderr, "'\\t': ");
			else
				fprintf(stderr, "0x%.2x: ", c);
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
			putc('\n', stderr);
	} while (c != EOF);
	nbytes--;
	if (verbose)
		fprintf(stderr, "FLSH: ");
	send(-1, 0, outfile);

	encode_free(e);

	if (verbose)
		fprintf(stderr, "\n%ld bytes -> %ld = 8 * %ld + %ld bits\n",
			nbytes, nbits, nbits / 8, nbits % 8);

	return 0;
}

static int decompress(FILE *infile, FILE *outfile)
{
	struct decoder *d;
	int cd;
	int bit_width, len;
	unsigned char *buf;

	d = decode_init(TABSIZE);
	bit_width = 8;
	do {
		if ((cd = receive(bit_width, infile)) < 0)
			break;
		if (verbose)
			fprintf(stderr, "(%.4x,%d):\t", cd, bit_width);
		if (cd == DEC_WIDTH) {
			if (verbose)
				fprintf(stderr, "DEC\n");
			bit_width--;
		} else if (cd == INC_WIDTH) {
			if (verbose)
				fprintf(stderr, "INC\n");
			bit_width++;
		} else {
			cd -= 2;
			len = decode(d, cd, &buf);
			fwrite(buf, 1, len, outfile);
			if (verbose) {
				int i;
				for (i = 0; i < len; i++)
					fprintf(stderr, " %.2x", buf[i]);
				putc('\n', stderr);
			}
		}
	} while (bit_width > 0);

	decode_free(d);

	return 0;
}

static void send(int code, int len, FILE *fp)
{
	static unsigned char bit_buf = 0;
	static int bits_free = 8;

	/* call send() with cd == -1 to flush the buffer */

	if (code < 0) {
		code = 0;
		len = bits_free != 8 ? bits_free : 0;
	}

	if (verbose)
		fprintf(stderr, " (%.4x,%d)", code, len);

	while (len > 0) {
		if (bits_free > len) {
			bit_buf |= code << (bits_free - len);
			bits_free -= len;
			len = 0;
		} else {
			bit_buf |= code >> (len - bits_free);
			len -= bits_free;
			putc(bit_buf, fp);
			bit_buf = 0;
			bits_free = 8;
		}
	}
}

static int receive(int len, FILE *fp)
{
	static unsigned char bit_buf;
	static int bits_used = 0;
	int code = 0;

	while (len > 0) {
		if (bits_used >= len) {
			code |= bit_buf >> (bits_used - len);
			bits_used -= len;
			len = 0;
		} else {
			int c;
			code |= bit_buf << (len - bits_used);
			len -= bits_used;
			if ((c = getc(fp)) == EOF)
				return -1;
			bit_buf = c;
			bits_used = 8;
		}
	}
	bit_buf &= (1 << bits_used) - 1;

	return code;
}
