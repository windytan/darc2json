/* a fast and beautiful envelope detector
 * [without LPF]
 *
 * (c) Oona Räisänen [windytan] 2013, ISC license
 * 
 * gcc -O3 -o env env.c
 * */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <fcntl.h>

#define IBUFLEN 256
#define OBUFLEN 1024

int main() {

  short int inbuf1[IBUFLEN], inbuf2[IBUFLEN],  outbuf[OBUFLEN];
  short int pow2[32768];
  int       obufptr=0, ibufptr=0, i, pipe1, pipe2, pipe3;
  
  for (i=0;i<32768;i++)
    pow2[i] = pow( 1.0*i/32768.0, 2 ) * 32767 + .5;

  pipe1 = open("pipe_02_split1", O_RDONLY);
  pipe2 = open("pipe_02_split2", O_RDONLY);
  pipe3 = open("pipe_03_env1",   O_WRONLY);

  while (1) {

    if (!read(pipe1, &inbuf1, 2*IBUFLEN)) return (EXIT_FAILURE);
    if (!read(pipe2, &inbuf2, 2*IBUFLEN)) return (EXIT_FAILURE);

    for (i=0; i<IBUFLEN; i++) {
      outbuf[obufptr++] = (inbuf1[i] > 0 ? pow2[inbuf1[i]] : pow2[-inbuf1[i]]);
      outbuf[obufptr++] = (inbuf2[i] > 0 ? pow2[inbuf2[i]] : pow2[-inbuf2[i]]);
  
      if (obufptr >= OBUFLEN) {
        if (!write(pipe3, &outbuf, 2*OBUFLEN)) return (EXIT_FAILURE);
        obufptr = 0;
      }
    }

  }
}
