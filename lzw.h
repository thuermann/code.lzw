/*
 * $Id: lzw.h,v 1.3 2015/02/18 23:32:43 urs Exp $
 */

#ifndef LZW_H
#define LZW_H

void code_init(void);
int code(int c, int *cd);

void decode_init(void);
int decode(int cd, unsigned char **buffer);

#endif
