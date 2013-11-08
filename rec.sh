#!/bin/sh
rtl_fm -N -f 87.9M -s 300000 -p 96 | sox -r 300k -t .raw -e signed -c 1 -b 16 - -r 300k test.wav
