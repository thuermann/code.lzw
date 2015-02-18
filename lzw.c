/*
 * lzw.c  -  Lempel-Zev-Welch coding scheme
 * see "A Technique for High Performance Data Compression",
 *     Terry A. Welch, IEEE Computer Vol 17, No 6 (June 1984), pp 8-19.
 *
 * $Id: lzw.c,v 1.5 2015/02/18 23:40:27 urs Exp $
 */

#include <stdlib.h>
#include <stdio.h>

#include "lzw.h"

#define TABSIZE (8192 - 2)
#define BSIZE   (TABSIZE - 256 + 1)

#define EMPTY    -1
#define NO_CHILD -1

typedef struct {
	int first_child;
	int next_child;
	unsigned char last;
} C_STRING;

struct encoder {
	C_STRING tab[TABSIZE];
	int curr_code;
	int next_free;
};

typedef struct {
	int prefix;
	unsigned char last;
	unsigned char first;
} D_STRING;

struct decoder {
	D_STRING tab[TABSIZE];
	int next_free;
	int last_code;
	unsigned char buffer[BSIZE];
};

static int search(const struct encoder *e, int prefix, unsigned char c);
static int insert(struct encoder *e, int prefix, unsigned char c);
static void flush(struct encoder *e);
static int write_code(const struct decoder *d, int cd, unsigned char *buffer);

struct encoder *encode_init(void)
{
	struct encoder *e;
	int i;

	if (!(e = malloc(sizeof(*e))))
		return NULL;

	for (i = 0; i < 256; i++) {
		e->tab[i].last = i;
		e->tab[i].first_child = NO_CHILD;
		e->tab[i].next_child  = NO_CHILD;
	}
	e->next_free = 256;
	e->curr_code = EMPTY;

	return e;
}

void encode_free(struct encoder *e)
{
	free(e);
}

int encode(struct encoder *e, int c, int *cd)
{
	int tmp;

	if (c == EOF) {
		*cd = e->curr_code;
		return 1;
	}

	if ((tmp = search(e, e->curr_code, c)) >= 0) {
		e->curr_code = tmp;
		return 0;
	}

	*cd = e->curr_code;

	if (tmp = insert(e, e->curr_code, c))
		flush(e);
	e->curr_code = c;

	return 1 + tmp;
}

static int search(const struct encoder *e, int prefix, unsigned char c)
{
	int cd;

	if (prefix < 0)
		return c;

	for (cd = e->tab[prefix].first_child; cd >= 0; cd = e->tab[cd].next_child)
		if (e->tab[cd].last == c)
			break;

	return cd;
}

static int insert(struct encoder *e, int prefix, unsigned char c)
{
	if (e->next_free == TABSIZE)
		return 1;

	e->tab[e->next_free].last = c;
	e->tab[e->next_free].first_child = NO_CHILD;
	e->tab[e->next_free].next_child = e->tab[prefix].first_child;
	e->tab[prefix].first_child = e->next_free;
	e->next_free++;

	return 0;
}

static void flush(struct encoder *e)
{
	int i;

	for (i = 0; i < 256; i++)
		e->tab[i].first_child = NO_CHILD;

	e->next_free = 256;
}

struct decoder *decode_init(void)
{
	struct decoder *d;
	int i;

	if (!(d = malloc(sizeof(*d))))
		return NULL;

	for (i = 0; i < 256; i++) {
		d->tab[i].prefix = EMPTY;
		d->tab[i].first  = d->tab[i].last = i;
	}
	d->next_free = 256;
	d->last_code = EMPTY;

	return d;
}

void decode_free(struct decoder *d)
{
	free(d);
}

int decode(struct decoder *d, int cd, unsigned char **cp)
{
	int len;

	if (d->last_code != EMPTY) {
		if (d->next_free == TABSIZE)
			d->next_free = 256;
		else {
			d->tab[d->next_free].prefix = d->last_code;
			d->tab[d->next_free].first = d->tab[d->last_code].first;
			d->tab[d->next_free].last = d->tab[cd].first;
			d->next_free++;
		}
	}

	len = write_code(d, cd, d->buffer + BSIZE);
	*cp = d->buffer + BSIZE - len;
	d->last_code = cd;

	return len;
}

static int write_code(const struct decoder *d, int cd, unsigned char *buffer)
{
	int len = 0;

	while (cd >= 256) {
		*--buffer = d->tab[cd].last;
		len++;
		cd = d->tab[cd].prefix;
	}
	*--buffer = cd;
	len++;

	return len;
}
