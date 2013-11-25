#!/bin/sh
rtl_fm -N -f 87.9M -l 0 -F -p 96 -s 180000 -g 25 |\
  sox -r 180k -t .raw -e signed -c 1 -b 16 - \
  -r 300k -t .raw - sinc -L 65000-87000 gain 10 > pipe_01_bp
