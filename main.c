#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "lzw.h"

#define DEC_WIDTH 0
#define INC_WIDTH 1

int compress(char *in, char *out);
int decompress(char *in, char *out);
void send(int cd, int bit_width, FILE *fp);
int receive(int bit_width, FILE *fp);

int verbose = 1;

int main(int argc, char **argv)
{
	int decompress_flag = 0;

	if (argc > 1 && strcmp(argv[1], "-d") == 0)
	{
		argc--, argv++;
		decompress_flag = 1;
	}
	if (decompress_flag)
		decompress(argv[1], argv[2]);
	else
		compress(argv[1], argv[2]);

	return (0);
}

int compress(char *in, char *out)
{
	int flag;
	int c, cd;
	int bit_width, next_width;
	long nbits = 0, nbytes = 0;
	FILE *infile, *outfile;

	if (!(infile = fopen(in, "rb")))
		return (-1);
	if (!(outfile = fopen(out, "wb")))
		return (-1);

	code_init();
	bit_width = 8;
	next_width = 256;
	do
	{
		c = fgetc(infile);
		nbytes++;
		if (verbose)
		{
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
		if (flag = code(c, &cd))
		{
			cd += 2;
			while (cd >= next_width)
			{
				send(INC_WIDTH, bit_width, outfile);
				nbits += bit_width;
				bit_width++;
				next_width *= 2;
			}
			send(cd, bit_width, outfile);
			nbits += bit_width;
			if (flag == 2)
				while (bit_width > 8)
				{
					send(DEC_WIDTH, bit_width, outfile);
					nbits += bit_width;
					bit_width--;
					next_width /= 2;
				}
		}
		if (verbose)
			printf("\n");
	}
	while (c != EOF);
	nbytes--;
	if (verbose)
		printf("FLSH: ");
	send(-1, 0, outfile);

	if (verbose)
		printf("\n%ld bytes -> %ld = 8 * %ld + %ld bits\n",
		       nbytes, nbits, nbits / 8, nbits % 8);

	fclose(infile);
	fclose(outfile);

	return (0);
}

int decompress(char *in, char *out)
{
	int cd;
	int bit_width, len;
	unsigned char *buf;
	FILE *infile, *outfile;

	if (!(infile = fopen(in, "rb")))
		return (-1);
	if (!(outfile = fopen(out, "wb")))
		return (-1);

	decode_init();
	bit_width = 8;
	do
	{
		if ((cd = receive(bit_width, infile)) < 0)
			break;
		if (verbose)
			printf("(%.4x,%d):\t", cd, bit_width);
		if (cd == DEC_WIDTH)
		{
			if (verbose)
				printf("DEC\n");
			bit_width--;
		}
		else if (cd == INC_WIDTH)
		{
			if (verbose)
				printf("INC\n");
			bit_width++;
		}
		else
		{
			cd -= 2;
			len = decode(cd, &buf);
			fwrite(buf, 1, len, outfile);
			if (verbose)
			{
				int i;
				for (i = 0; i < len; i++)
					printf(" %.2x", buf[i]);
				putchar('\n');
			}
		}
	}
	while (bit_width > 0);

	fclose(infile);
	fclose(outfile);

	return (0);
}

typedef unsigned int WORD;
#define WSIZE (8 * sizeof(WORD))

void send(int cd, int bit_width, FILE *fp)
{
	static int bits_free = WSIZE;
	static WORD buf = 0;

	/* call send() with cd == -1 to flush the buffer */

	if (cd < 0)
	{
		cd = 0;
		bit_width = bits_free;
	}

	if (verbose)
		printf(" (%.4x,%d)", cd, bit_width);

	if (bits_free > bit_width)
	{
		buf |= cd << (bits_free - bit_width);
		bits_free -= bit_width;
	}
	else
	{
		buf |= cd >> (bit_width - bits_free);
		fwrite(&buf, sizeof(buf), 1, fp);
		buf = bits_free == bit_width ?
			0 : cd << (WSIZE - (bit_width - bits_free));
		bits_free = WSIZE - (bit_width - bits_free);
	}
}

int receive(int bit_width, FILE *fp)
{
	static int bits_used = 0;
	static WORD buf;
	int cd;

	if (bits_used >= bit_width)
	{
		cd = buf >> (bits_used - bit_width);
		bits_used -= bit_width;
	}
	else
	{
		cd = buf << (bit_width - bits_used);
		if (fread(&buf, sizeof(buf), 1, fp) != 1)
			return -1;
		cd |= buf >> (WSIZE - (bit_width - bits_used));
		bits_used = WSIZE - (bit_width - bits_used);
	}
	return cd & ((1 << bit_width) - 1);
}
