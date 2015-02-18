/*
 * lzw.c  -  Lempel-Zev-Welch coding scheme
 * see "A Technique for High Performance Data Compression",
 *     Terry A. Welch, IEEE Computer Vol 17, No 6 (June 1984), pp 8-19.
 *
 * $Id: lzw.c,v 1.7 2015/02/18 23:43:14 urs Exp $
 */

#include <stdlib.h>
#include <stdio.h>

#include "lzw.h"

#define EMPTY    -1
#define NO_CHILD -1

typedef struct {
	int first_child;
	int next_child;
	unsigned char last;
} C_STRING;

struct encoder {
	int size;
	C_STRING *tab;
	int curr_code;
	int next_free;
};

typedef struct {
	int prefix;
	unsigned char last;
	unsigned char first;
} D_STRING;

struct decoder {
	int size;
	D_STRING *tab;
	int next_free;
	int last_code;
	unsigned char *buffer;
};

static int search(const struct encoder *e, int prefix, unsigned char c);
static int insert(struct encoder *e, int prefix, unsigned char c);
static void flush(struct encoder *e);
static int write_code(const struct decoder *d, int cd, unsigned char *buffer);

struct encoder *encode_init(int tabsize)
{
	struct encoder *e;
	int i;

	if (!(e = malloc(sizeof(*e))))
		return NULL;
	if (!(e->tab = malloc(tabsize * sizeof(e->tab[0])))) {
		free(e);
		return NULL;
	}
	e->size = tabsize;

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
	free(e->tab);
	free(e);
}

int encode(struct encoder *e, int c, int *cd)
{
	int code, flag;

	if (c == EOF) {
		*cd = e->curr_code;
		return 1;
	}

	if ((code = search(e, e->curr_code, c)) >= 0) {
		e->curr_code = code;
		return 0;
	}

	*cd = e->curr_code;

	if (flag = insert(e, e->curr_code, c))
		flush(e);
	e->curr_code = c;

	return 1 + flag;
}

static int search(const struct encoder *e, int prefix, unsigned char c)
{
	C_STRING *tab = e->tab;
	int cd;

	if (prefix < 0)
		return c;

	for (cd = tab[prefix].first_child; cd >= 0; cd = tab[cd].next_child)
		if (tab[cd].last == c)
			break;

	return cd;
}

static int insert(struct encoder *e, int prefix, unsigned char c)
{
	C_STRING *tab = e->tab;
	int new;

	if (e->next_free == e->size)
		return 1;

	new = e->next_free++;
	tab[new].last        = c;
	tab[new].first_child = NO_CHILD;
	tab[new].next_child  = tab[prefix].first_child;
	tab[prefix].first_child = new;

	return 0;
}

static void flush(struct encoder *e)
{
	int i;

	for (i = 0; i < 256; i++)
		e->tab[i].first_child = NO_CHILD;

	e->next_free = 256;
}

struct decoder *decode_init(int tabsize)
{
	struct decoder *d;
	int i;

	if (!(d = malloc(sizeof(*d))))
		return NULL;
	if (!(d->tab = malloc(tabsize * sizeof(d->tab[0])))) {
		free(d);
		return NULL;
	}
	if (!(d->buffer = malloc(tabsize - 256 + 1))) {
		free(d->tab);
		free(d);
		return NULL;
	}
	d->size = tabsize;

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
	free(d->buffer);
	free(d->tab);
	free(d);
}

int decode(struct decoder *d, int cd, unsigned char **cp)
{
	D_STRING *tab = d->tab;
	unsigned char *end;
	int len;

	if (d->last_code != EMPTY) {
		if (d->next_free == d->size)
			d->next_free = 256;
		else {
			int new = d->next_free++;
			tab[new].prefix = d->last_code;
			tab[new].first  = tab[d->last_code].first;
			tab[new].last   = tab[cd].first;
		}
	}

	end = d->buffer + d->size - 256 + 1;
	len = write_code(d, cd, end);
	*cp = end - len;
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
