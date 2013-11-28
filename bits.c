/* Oona Räisänen [windytan] 2013, ISC license
 *
 * gcc -o bits bits.c
 */

#include <stdio.h>

#define IBUFLEN 1024
#define OBUFLEN 1024

int main() {

  double    fs = 300000;
  double    bps = 16000;
  double    bitphase = 0;
  short int pcm, prevpcm=0, ibuf[IBUFLEN] = {0};
  double    Tb  = 1.0/bps;
  double    Tb2 = 1.0*Tb/2.0;
  double    Ts  = 1.0/fs;
  int       ibufptr=0,obufptr=0;
  char      obuf[OBUFLEN] = {0};


  while (fread(ibuf, 2, IBUFLEN, stdin)) {

    for (ibufptr = 0; ibufptr < IBUFLEN; ibufptr++) {
      pcm = ibuf[ibufptr];
      bitphase += Ts;

      if (bitphase >= Tb) {
        bitphase -= Tb;
        obuf[obufptr++] = (pcm > 0 ? '0' : '1');
        if (obufptr >= OBUFLEN) {
          printf("%.*s", OBUFLEN, obuf);
          obufptr = 0;
        }
      }

      if ((prevpcm < 0 && pcm >= 0) || (prevpcm >= 0 && pcm < 0) ) {
        if (bitphase > Tb2) {
          bitphase -= bitphase * .05;
        } else if (bitphase < 1.0*Tb/2.0) {
          bitphase += bitphase * .05;
        }
      }
      prevpcm = pcm;
    }

  }
}
