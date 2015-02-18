/*
 * lzw.c  -  Lempel-Zev-Welch coding scheme
 * see "A Technique for High Performance Data Compression",
 *     Terry A. Welch, IEEE Computer Vol 17, No 6 (June 1984), pp 8-19.
 *
 * $Id: lzw.c,v 1.4 2015/02/18 23:32:53 urs Exp $
 */

#include <stdio.h>

#include "lzw.h"

#define TABSIZE (8192 - 2)

#define EMPTY    -1
#define NO_CHILD -1

static int search(int prefix, unsigned char c);
static int insert(int prefix, unsigned char c);
static void flush(void);
static int write_code(int cd, unsigned char *buffer);

typedef struct {
	int first_child;
	int next_child;
	unsigned char last;
} C_STRING;

typedef struct {
	int prefix;
	unsigned char last;
	unsigned char first;
} D_STRING;

static C_STRING code_tab[TABSIZE];
static D_STRING decode_tab[TABSIZE];

static int c_next_free;
static int d_next_free;
static int curr_code;
static int last_code;

void code_init(void)
{
	int i;

	for (i = 0; i < 256; i++) {
		code_tab[i].last = i;
		code_tab[i].first_child = NO_CHILD;
		code_tab[i].next_child  = NO_CHILD;
	}
	c_next_free = 256;
	curr_code = EMPTY;
}

int code(int c, int *cd)
{
	int tmp;

	if (c == EOF) {
		*cd = curr_code;
		return 1;
	}

	if ((tmp = search(curr_code, c)) >= 0) {
		curr_code = tmp;
		return 0;
	}

	*cd = curr_code;

	if (tmp = insert(curr_code, c))
		flush();
	curr_code = c;

	return 1 + tmp;
}

static int search(int prefix, unsigned char c)
{
	int cd;

	if (prefix < 0)
		return c;

	for (cd = code_tab[prefix].first_child; cd >= 0; cd = code_tab[cd].next_child)
		if (code_tab[cd].last == c)
			break;

	return cd;
}

static int insert(int prefix, unsigned char c)
{
	if (c_next_free == TABSIZE)
		return 1;

	code_tab[c_next_free].last = c;
	code_tab[c_next_free].first_child = NO_CHILD;
	code_tab[c_next_free].next_child = code_tab[prefix].first_child;
	code_tab[prefix].first_child = c_next_free;
	c_next_free++;

	return 0;
}

static void flush(void)
{
	int i;

	for (i = 0; i < 256; i++)
		code_tab[i].first_child = NO_CHILD;

	c_next_free = 256;
}

void decode_init(void)
{
	int i;

	for (i = 0; i < 256; i++) {
		decode_tab[i].prefix = EMPTY;
		decode_tab[i].first  = decode_tab[i].last = i;
	}
	d_next_free = 256;
	last_code = EMPTY;
}

int decode(int cd, unsigned char **cp)
{
#define BSIZE (TABSIZE-255)

	int len;
	static unsigned char buffer[BSIZE];

	if (last_code != EMPTY) {
		if (d_next_free == TABSIZE)
			d_next_free = 256;
		else {
			decode_tab[d_next_free].prefix = last_code;
			decode_tab[d_next_free].first = decode_tab[last_code].first;
			decode_tab[d_next_free].last = decode_tab[cd].first;
			d_next_free++;
		}
	}

	len = write_code(cd, buffer + BSIZE);
	*cp = buffer + BSIZE - len;
	last_code = cd;

	return len;
}

static int write_code(int cd, unsigned char *buffer)
{
	int len = 0;

	while (cd >= 256) {
		*--buffer = decode_tab[cd].last;
		len++;
		cd = decode_tab[cd].prefix;
	}
	*--buffer = cd;
	len++;

	return len;
}
