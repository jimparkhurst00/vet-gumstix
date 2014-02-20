#include <stdio.h>
#include <string.h>
#include "rand.h"

unsigned char randomBuffer[RANDOM_BUFFER_SIZE]={0};
unsigned int randomIndex=0;

unsigned long long refillNo=0;
unsigned char randkey[RANDOM_BUFFER_SIZE]={0};
arc4_context arc4_ctx={0};


int random_byte(void) {
  int ret;
  if (!(randomIndex % RANDOM_BUFFER_SIZE))
    {
      ret = arc4GetRandom(randomBuffer, RANDOM_BUFFER_SIZE);
    }
  return randomBuffer[randomIndex++ % RANDOM_BUFFER_SIZE];
}


int arc4GetRandom(unsigned char *buffer, int numBytes) {
  int ret;
  int nbytes;
  int i;
  if(!refillNo) {

    memset(randkey, 0xa, RANDOM_BUFFER_SIZE);

    nbytes = RANDOM_BUFFER_SIZE;
    if(nbytes != RANDOM_BUFFER_SIZE)
      return 0;
    arc4_ctx.x = 0;
    arc4_ctx.y = 0;
    arc4_setup(&arc4_ctx, randkey, RANDOM_BUFFER_SIZE);
  }

  memset(buffer, 0, RANDOM_BUFFER_SIZE);
  memcpy(buffer, &refillNo, sizeof(unsigned long long));

  arc4_crypt(&arc4_ctx, buffer, numBytes);
  refillNo++;
  return numBytes;
}
