typedef short int CODE;

void code_init(void);
int code(int c, int *cd);

void decode_init(void);
int decode(int cd, unsigned char **buffer);
