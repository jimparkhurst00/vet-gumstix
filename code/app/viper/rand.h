
#include "arc4.h"

#define RANDOM_BUFFER_SIZE 128

int random_byte(void);
int arc4GetRandom(unsigned char *buffer, int numBytes);
