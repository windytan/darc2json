#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#define BUFLEN 2048

int main() {

  short int lo;
  short int hi;
  short int buf[BUFLEN];
  unsigned short bufptr = 0;

  /* Actual signal */
  while (read(0, &lo, 2)) {

    read(0,&hi,2);

    buf[bufptr++] = pow(lo/32768.0,2) * 327680000.0;
    buf[bufptr++] = pow(hi/32768.0,2) * 327680000.0;

    if (bufptr == BUFLEN) {
      write(1, &buf, 2 * BUFLEN);
      bufptr = 0;
    }

  }
}
