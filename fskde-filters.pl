use warnings;
use List::Util qw(sum);
use Term::ANSIColor;
use Digest::CRC qw(crc);
use 5.010;

$input_wav = "neljas.wav";

@mseq = qw( 1 0 1 0 1 1 1 1 1 0 1 0 1 0 1 0 1 0 0 0 0 0 0 1 0 1 0 0 1 0 1 0 1 1 1 1 0 0 1 0 1 1 1 0 1 1 1 0 0 0 0 0 0 1 1 1 0 0 1 1 1 0 1 0 0 1 0 0 1 1 1 1 0 1 0 1 1 1 0 1 0 1 0 0 0 1 0 0 1 0 0 0 0 1 1 0 0 1 1 1 0 0 0 0 1 0 1 1 1 1 0 1 1 0 1 1 0 0 1 1 0 1 0 0 0 0 1 1 1 0 1 1 1 1 0 0 0 0 1 1 1 1 1 1 1 1 1 0 0 0 0 0 1 1 1 1 0 1 1 1 1 1 0 0 0 1 0 1 1 1 0 0 1 1 0 0 1 0 0 0 0 0 1 0 0 1 0 1 0 0 1 1 1 0 1 1 0 1 0 0 0 1 1 1 1 0 0 1 1 1 1 1 0 0 1 1 0 1 1 0 0 0 1 0 1 0 1 0 0 1 0 0 0 1 1 1 0 0 0 1 1 0 1 1 0 1 0 1 0 1 1 1 0 0 0 1 0 0 1 1 0 0 0 1 0 0 0 1 0 0 0 0 0 0 0 0 1 0 0 0 0 1 0 0 0 1 1 0 0 0 0 1 0 0 1 1 1 0 0 1);

$|++;
$fs = 300000;
$fc = 76000;
$bps = 16000;

$Tb = 1/$bps;

%bics = (0b0001001101011110 => 1,
         0b0111010010100110 => 2,
         0b1010011110010001 => 3,
         0b1100100001110101 => 4);

sub detect {
  print "darc bandpass\n";
  system("sox $input_wav 1_bandpass.wav sinc -L 66000-86000");

  print "mark/space split\n";
  system("sox 1_bandpass.wav 2_split_lo.wav sinc -$fc");
  system("sox 1_bandpass.wav 2_split_hi.wav sinc $fc");

  print "envelope\n";
  open(S1,"sox 2_split_lo.wav -t .raw -|");
  open(S2,"sox 2_split_hi.wav -t .raw -|");

  open(U,"|sox -t .raw -b 16 -e signed -r $fs -c 2 - 3_envelope_filtered.wav sinc -$bps");
  while (not eof S1) {

    read(S1,$a,2);
    read(S2,$b,2);

    $a = ((unpack("s",$a)/32768) ** 2) * 327680000;
    $b = ((unpack("s",$b)/32768) ** 2) * 327680000;
    
    print U pack("s",$a);
    print U pack("s",$b);

  }
  close(S1);
  close(S2);
  close(U);

  print "difference\n";
  open(S,"sox 3_envelope_filtered.wav -t .raw -|");
  open(U,"|sox -c 1 -b 16 -e signed -r 300k -t .raw - 4_difference.wav");
  while (not eof S) {
    read(S,$a,2);
    read(S,$b,2);
    print U pack("s", unpack("s",$a)-unpack("s",$b));
  }
  close(U);
  close(S);
}
#detect();

print "bits\n";
open(S,"sox 4_difference.wav -t .raw -|");
open(U,"|sox -t .raw -b 16 -e signed -r $fs -c 2 - 5_bits.wav");
while (not eof S) {
  read(S,$a,2);
  $a = unpack("s",$a);

  $bittime += 1/$fs;
  if ($bittime >= $Tb) {
    $bittime -= $Tb;
    layer2($a > 0 ? 0 : 1);
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

sub layer2 {
  my $bit = shift;

  $bit = descra($bit) if ($insync && @words > 0);

  $shifter = ((($shifter//0) << 1) + $bit) & 0xffff;

  if (!$insync) {

    if (exists($bics{$shifter})) {
      $insync = 1;
      $wordphase = 0;
      @words = ();
      scramble_init();
    }
  }

  if ($insync && $wordphase % 16 == 0) {

    push(@words, $shifter);
    if (@words == 18) {
      $wnum ++;
      $bic = shift(@words);
      print "\n";
      if (exists $bics{$bic}) {
        print "BIC".$bics{$bic}." ";
      } else {
        print "???? ";
        $insync=0;
      }

      if (($bics{$bic} // 0) == 4) {
        print "(parity)\n";
      } else {
        @data = @words[0..10];
        $dstring = "";
        $dstring .= chr($_>>8) . chr($_ & 0xff) for (@data);
        $crc = $words[11] >> 2;

        $calc_crc = crc($dstring,14,0x0000,0x0000,0,0x0805,0,0);
        $haserror = ($crc != $calc_crc);
        print "info:";
        printf("%04x ",$_) for (@data);
        print " crc:";
        print colored([$haserror ? 'red' : 'green'],sprintf("%04x",$crc));

        print " parity:\n";

        layer3($haserror, @data);
        
      }
      $wordphase = 0;
      @words = ();
      scramble_init();
    }
  }

  $wordphase ++;

}

sub layer3 {
  my $haserror = shift;
  my @data = @_;
  $SILCh = field($data[0],0,4);
  printf ("SI/LCh: 0x%X ",$SILCh); 
  if ($SILCh == 0x8) {
    print "Service Channel (SeCH)\n";
    $lf   = field($data[0],5,1);
    $dup  = field($data[0],6,2);
    $cid  = field($data[0],8,4);
    $type = field($data[0],12,4);
    $nid  = field($data[1],0,4);
    $bln  = field($data[1],4,4);
    print "Last Fragment\n" if ($lf);
    print "Dup: $dup\n";
    printf("CID: %x\n",$cid);
    print "Type: $type ";
    given($type) {
      when (0b0000) { print "Channel Organization Table (COT)\n"; }
      when (0b0001) { print "Alternative Frequency Table (AFT)\n"; }
      when (0b0010) { print "Service Alternative Frequency Table (SAFT)\n"; }
      when (0b0011) { print "Time, Date, Position and Network name Table (TDPNT)\n"; }
      when (0b0100) { print "Service Name Table (SNT)\n"; }
      when (0b0101) { print "Time and Date Table (TDT)\n"; }
      when (0b0110) { print "Synchronous Channel Organization Table (SCOT)\n"; }
      default { print colored(['red'],"err\n"); }
    }
    print "Network ID: $nid\n";
    print "Block #$bln\n";
 
    @bytes = ();
    push(@bytes,field($data[1],8,8));
    for (2..10) {
      push(@bytes,field($data[$_],0,8));
      push(@bytes,field($data[$_],8,8));
    }
    
    if ($bln == 0) {
      %servmsgbuf = ();
      $servmsgbuf{'ml'}    = (($bytes[1] & 1) << 8) + $bytes[2];
      $servmsgbuf{'ecc'}   = $bytes[0];
      $servmsgbuf{'tseid'} = $bytes[1] >> 1;
      $servmsgbuf{'type'}  = $type;
    }
    push(@{$servmsgbuf{'data'}},$_) for (@bytes);
    $servmsgbuf{'prevblock'} = $bln;
    $servmsgbuf{'errors'} .= $haserror;

    if ($lf) {
      servmsg();
    }
    
   } elsif ($SILCh == 0xA) {
    print "Long Message Channel (LMCh)\n";
    $di = field($data[0],4,1);
    $lf = field($data[0],5,1);
    $sc = field($data[0],6,4);
    $lm_crc = field($data[0],10,6);
    $calc_lm_crc = crc(pack("S",$data[0]>>6),6,0x0,0x0,0,0b1011001,0,0);
    printf("crc: %06b (calc: %06b)\n",$lm_crc,$calc_lm_crc);
    print "sc:$sc\n";
    print "Real-Time\n" if ($di);
    print "Last Fragment\n" if ($lf);
    shift(@data);
    $dta = "";
    for (@data) {
      $dta .= chr(field($_,0,8));
      $dta .= chr(field($_,8,8));
    }
    print "Data: ";
    printsafe ($dta);
    print "\n";
  } elsif ($SILCh == 0xB) {
    print "Block Message Channel (BMCh)\n";
    $SCh = field($data[0],5,3);
    print "Sub-Channel: $SCh ";
    if ($SCh == 0) {
      print "Block Application Channel\n";
    } elsif ($SCh == 4) {
      print "Synchronous Frame Message\n";
      $fno = field($data[1],3,5);
      print "Frame #$fno\n";

    } else {
      print colored(['red'],"err\n");
    }
  } else {
    print colored(['red'],"err");
  }
}

sub servmsg {
  print "Service Message (";
  if ($servmsgbuf{'errors'}) {
    print colored(['red'],"errors");
  } else {
    print colored(['green'],"ok");
  }
  
  print ") [[\n";
  @bytes = @{$servmsgbuf{'data'}};
  printf("  ECC: %02x\n",$servmsgbuf{'ecc'});
  printf("  TSEID: %02x\n",$servmsgbuf{'tseid'});
  printf("  Message Length: %d bytes\n",$servmsgbuf{'ml'});
  given ($servmsgbuf{'type'}) {
    when (0b0000) {
      print "  Channel Organization Table (COT)\n";
      my $n = 3;
      while (defined $bytes[$n] && $bytes[$n] != 0) {
        $sid = ($bytes[$n] << 8) + ($bytes[$n+1] >> 2);
        print "  Service Identity: ".sprintf("%04x\n",$sid);
        $ca = ($bytes[$n+1] >> 1) & 1;
        print "    is ".($ca ? "" : "not ")."encrypted\n";
        $ca = ($bytes[$n+1]) & 1;
        print "    is ".($ca ? "" : "not ")."currently available\n";
        $n += 2 + $ca;
      }
    }
    when (0b0001) {
      $ft = $bytes[3] >> 6;
      print "  Alt. Frequencies for Frame ".qw( A0 A1 B C )[$ft]."\n";
      $afnum = $bytes[3] & 0b111111;
      $tf = dec_af($bytes[4]);
      print "    Tuned Frequency: $tf MHz\n";
      @afs = ();
      for (0..$afnum-1) {
        print "    Alt.  Frequency: ".dec_af($bytes[5+$_])." MHz\n";
      }

    }
    when (0b0101) {
      print "  Time and Date Table (TDT)\n";
      @time = ($bytes[3],$bytes[4],$bytes[5],$bytes[6]);
      @date = ($bytes[7],$bytes[8],$bytes[9]);
      @nname = ($bytes[10],$bytes[11],$bytes[12]);
      $mjd = (($date[0] & 0b1111111) << 10) +
             ($date[1] << 2) +
             ($date[2] >> 6);

      $year  = int(($mjd - 15078.2)/365.25);
      $month = int(($mjd-14956.1-int($year * 365.25))/30.6001);
      $day   = $mjd - 14956-int($year*365.25)-int($month*30.6001);
      if ($month == 14 || $month == 15) {
        $k = 1;
      } else {
        $k = 0;
      }
      $year += $k;
      $month = $month - 1 - $k*12;

      printf("  Time: %04d-%02d-%02d %02d:%02d:%02d\n",
        $year+1900, $month, $day,
        ($time[0]>>2)&0b11111,
        (($time[0]&0b11)<<4) + ($time[1] >>4),
        (($time[1]&0b1111)<<2) + ($time[2]>>6));

      $nnl = ($bytes[9]>>2) & 0b1111;
      $nname = "";
      $nname .= chr($bytes[10+$_]) for (0..$nnl-1);
      print "  Network name: \"$nname\"\n";

      $pf = ($bytes[9] >> 1) & 0b01;
      if ($pf) {
        print "  Position\n";
        $freq = dec_af($bytes[10+$nnl]);
        print "  Freq: $freq\n";
      }
    }
    when (0b0110) {
      print "  Synchronous Channel Organization Table (SCOT)\n";
      my $n = 3;
      while (defined $bytes[$n] && $bytes[$n] != 0) {
        $ext = $bytes[$n] >> 7;
        if ($ext) {
          $sid = ( ($bytes[$n] & 0b1111111) << 7) + ($bytes[$n+1] >> 1);
        } else {
          $sid = $bytes[$n] & 0b1111111;
        }
        print "  Service Identity: ".sprintf("%04x\n",$sid);
        $cy = ($bytes[$n+2] >> 5) & 0b111;
        @cycles = ("frame 0,2,4,6,8,10,12,14,16,18,20,22","frame 0,3,6,9,12,15,18,21","frame 0,4,8,12,16,20","frame 0,6,12,18","frame 0,8,16","frame 0,12","frame 0");
        print "  Cycle: $cycles[$cy]\n";
        $n = $n + 1 + $ext;
      }

    }
  }


  print "]]\n";
}

sub scramble_init {
  $scramble_ptr = 0;
}

sub dec_af {
  if ($_[0] >= 1 && $_[0] <= 204) {
    return 87.5 + $_[0]*.1;
  } else {
    return "err";
  }
}

sub descra {
  $scramble_ptr++;
  $_[0] ^ $mseq[$scramble_ptr-1];
}

sub field {
  my $wd = shift;
  my $start = shift;
  my $len = shift;
  my $fld = ($wd >> (16-$start-$len)) & (2**$len-1);
  my $fld2=0;
  for (0..$len-1) {
    $fld2 += (1<<($len-$_-1)) if (($fld >> $_) & 1);
  }
  $fld2;
}

sub printsafe {
  my @chars = split(//,$_[0]);
  for (@chars) {
    if (ord($_) > 31 && ord($_)<127) {
      print $_;
    } else {
      print ".";
    }
  }
}
