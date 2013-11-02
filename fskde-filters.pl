use warnings;
use List::Util qw(sum);
use Term::ANSIColor;
$|++;
$fs = 300000;
$fc = 76000;
$bps = 16000;

$Tb = 1/$bps;

$pi = 3.141592653589793;

print "darc bandpass\n";
system("sox neljas.wav neljas_300k.wav sinc -L 66000-86000");

print "mark/space split\n";
system("sox neljas_300k.wav neljas_300k_a.wav sinc -L -$fc");
system("sox neljas_300k.wav neljas_300k_b.wav sinc -L $fc");

print "difference\n";
open(S1,"sox neljas_300k_a.wav -t .raw -|");
open(S2,"sox neljas_300k_b.wav -t .raw -|");

open(U,"|sox -t .raw -b 16 -e signed -r $fs -c 1 - neljas_filters.wav sinc -$bps");
while (not eof S1) {

  read(S1,$a,2);
  read(S2,$b,2);

  $a = ((unpack("s",$a)/32768) ** 2) * 327680000;
  $b = ((unpack("s",$b)/32768) ** 2) * 327680000;
  
  print U pack("s",$a-$b);

}
close(S1);
close(S2);
close(U);

print "bits\n";
open(S,"sox neljas_filters.wav -t .raw -|");
open(U,"|sox -t .raw -b 16 -e signed -r $fs -c 2 - neljas_filters_bits.wav");
while (not eof S) {
  read(S,$a,2);
  $a = unpack("s",$a);

  $bittime += 1/$fs;
  if ($bittime >= $Tb) {
    $bittime -= $Tb;
    $bit = ($a > 0 ? 0 : 1);
    print $bit;
    $shifter = ((($shifter//0) << 1) + $bit) & 0xffff;
    if ($shifter == 0b0001001101011110 ||
        $shifter == 0b0111010010100110 ||
        $shifter == 0b1010011110010001 ||
        $shifter == 0b1100100001110101) {
      print colored(['green']," !! ");
    }
  }
  if (($preva//0) * $a < 0) {
    if ($bittime > $Tb/2) {
      $bps -= ($bittime-$Tb/2)/$Tb*.1;
    } elsif ($bittime < $Tb/2) {
      $bps += ($Tb/2-$bittime)/$Tb*.1;
    }
    $Tb = 1/$bps;
  }
  print U pack("s",$a);
  print U pack("s",$bittime/$Tb*10000);
  #print "$bps\n";
  $preva = $a;
}
close(S);
