/*
 * $Id: lzw.h,v 1.4 2015/02/18 23:40:27 urs Exp $
 */

#ifndef LZW_H
#define LZW_H

struct encoder;
struct decoder;

struct encoder *encode_init(void);
void encode_free(struct encoder *e);
int  encode(struct encoder *e, int c, int *cd);

struct decoder *decode_init(void);
void decode_free(struct decoder *d);
int  decode(struct decoder *d, int cd, unsigned char **cp);

#endif
