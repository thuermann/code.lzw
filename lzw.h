/*
 * $Id: lzw.h,v 1.5 2015/02/18 23:42:39 urs Exp $
 */

#ifndef LZW_H
#define LZW_H

struct encoder;
struct decoder;

struct encoder *encode_init(int tabsize);
void encode_free(struct encoder *e);
int  encode(struct encoder *e, int c, int *cd);

struct decoder *decode_init(int tabsize);
void decode_free(struct decoder *d);
int  decode(struct decoder *d, int cd, unsigned char **cp);

#endif
