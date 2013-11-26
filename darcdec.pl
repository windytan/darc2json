#!/usr/bin/perl
# Oona Räisänen [windytan] 2013, ISC license
#
# Note: This script will create a bunch of FIFOs and
# background processes

use warnings;
use Term::ANSIColor;
use Encode qw(encode decode);
use 5.010;

$|++;
$fs  = 300000;
$fc  = 76000;
$bps = 16000;

$Tb = 1/$bps;

%bics = (0xa791 => 1,
         0x135e => 2,
         0xc875 => 3,
         0x74a6 => 4);
@bickeys = keys %bics;


if (-e "syndrome82") {
  open(S,"syndrome82");
  while (<S>) {
    chomp();
    ($a,$b) = split(/ /,$_);
    $errstring{$a} = pack("H*",$b);
  }
  close(S);

} else {
  print "syndrome precalc\n";

  @estrings = ([1,0b1],[2,0b11]);
  for $len (3..10) {
    for $n (0..2**($len-2)-1) {
      $es = (1<<($len-1)) + ($n << 1) + 1;
      push (@estrings, [$len,$es]);
    }
  }

  for $len (11..24) {
    $es = (1<<($len-1)) + 1;
    push(@estrings, [$len,$es]);
  }

  open(U,">syndrome82");
  for (@estrings) {
    ($len,$es) = @{$_};
    printf("  ($len-bit %010b)\n",$es);
    for $shft (0..190-$len) {
      $bits= ("0" x (190-$shft-$len)) . sprintf("%b",$es) . ("0" x $shft);
      $str = pack("B*",$bits);
      $sy = crc82($str, chr(0) x 10);

      # shift 2 left
      $bits = "00".substr(unpack("B*",$str),0,-2);
      $str = pack("B*",$bits);
      $errstring{$sy} = $str;

      print U "$sy ".unpack("H48",$str)."\n";
    
    }
  }
  close(U);
}

system("./rec.sh &");

sleep(7);

$format  = "-t .raw -r 300k -c 1 -e signed -b 16";
$sformat = "-t .raw -r 300k -c 2 -e signed -b 16";

for (qw( pipe_01_bp pipe_02_split1 pipe_02_split2 pipe_03_env1 pipe_03_env2 pipe_04_env_st)) {
  system("mkfifo $_") unless (-e $_);
}

print "mark/space split\n";
system("tee pipe_01_bp2 < pipe_01_bp | sox $format - $format pipe_02_split1 sinc -$fc &");
system("sox $format pipe_01_bp2 $format pipe_02_split2 sinc $fc &");

print "envelope\n";
system("sox $sformat pipe_03_env1 $sformat pipe_04_env_st sinc -L -$bps &");
sleep(4);
system("./env &");

print "difference\n";
open(S,"sox $sformat pipe_04_env_st -c 1 -t .raw - oops | ./bits |");

print "bits\n";

while (not eof S) {
  read(S,$a,1);
  my $bit = 0+$a;

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
      print "\n";
      $bic = shift(@words);
      if (not exists $bics{$bic}) {
        undef $bicnum;

        # etsi lähin
        for $b (0..3) {
          $t = $bic ^ $bickeys[$b];
          $n = (unpack '%8b*',pack 'S>',$t);
          if ($n <= 2) {
            $bicnum = $b+1;
            last;
          }
        }
      } else {
        $bicnum = $bics{$bic};
      }

      if (defined $bicnum) {
        print "BIC$bicnum ";
      } else {
        print "???? ";
        $insync=0;
      }

      if (($bicnum // 0) == 4) {
        print "(parity)";
      } elsif (not defined $bicnum) {
        print "\n";
      } else {
        @data = @words[0..10];
        $dstring = "";
        $dstring .= chr($_>>8) . chr($_ & 0xff) for (@data);
        $crc = $words[11] >> 2;

        $synd = crc14($dstring,pack("S>",$crc));
        $haserror = ($synd eq "0000" ? 0 : 1);

        print "info:";
        printf("%04x ",$_) for (@data);
        print " crc:";
        print colored([$haserror ? 'red' : 'green'],sprintf("%04x",$crc));
        print " (synd=$synd)";

        print "\nparity:";
        $parstring = chr($words[11] & 0b11);
        $parstring .= chr($words[$_]>>8) . chr($words[$_] & 0xff) for (12..16);
        printf("%02x",ord(substr($parstring,$_,1))) for (0..length($parstring)-1);

        $dstring = "";
        $dstring .= chr($_>>8) . chr($_ & 0xff) for (@words[0..11]);

        if ($haserror) {
          $my_par = crc82($dstring,$parstring);
          print " (synd=$my_par)";
          if (exists $errstring{$my_par}) {
            $e = $errstring{$my_par};
            for (0..11) {
              $words[$_] ^= unpack("S>",substr($e,$_*2,2));
            }
            @data = @words[0..10];
    
            # recalc crc
            $dstring = "";
            $dstring .= chr($_>>8) . chr($_ & 0xff) for (@data);
            $crc = $words[11] >> 2;

            $calc_synd = crc14($dstring,pack("S>",$crc));
            $haserror = ($calc_synd eq "0000" ? 0 : 1);

            if (!$haserror) {
              print colored(['green']," ftfy :)");
              print " synd=$calc_synd";
            } else {
              print colored(['red']," fix fails!");
            }
          } else {
            print "  uncorrectable";
          }

        }

        print "\n";
        layer3($haserror, @data);
        
      }
      $wordphase = 0;
      @words = ();
      scramble_init();
    }
  }

  $wordphase ++;

}

close(S);

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

  } elsif ($SILCh == 0x9) {
    print "Short Message Channel (SMCh)\n";
    $di = field($data[0],4,1);
    $lf = field($data[0],5,1);
    $sc = field($data[0],6,4);
    $sm_crc = field($data[0],10,6);
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
    
  } elsif ($SILCh == 0xA) {
    print "Long Message Channel (LMCh)\n";
    $di = field($data[0],4,1);
    $lf = field($data[0],5,1);
    $sc = field($data[0],6,4);
    $lm_crc = $data[0] & 0b111111;
    $calc_synd = crc6(pack("S>",$data[0]), chr($lm_crc), 6);
    printf("L3-hdr-crc: %02x (synd: $calc_synd)\n",$lm_crc);
    print "sc:$sc\n";
    if ($sc != ((($prev_sc//0)+1) % 16)) {
      %lmsg = ();
    }

    print "Real-Time\n" if ($di);
    print "Last Fragment\n" if ($lf);
    shift(@data);
    $dta = "";
    for (@data) {
      $dta .= chr(field($_,0,8));
      $dta .= chr(field($_,8,8));
    }
    $lmsg{'data'} .= $dta;
    $lmsg{'errors'} .= $haserror;

    print "Data: ";
    printsafe ($dta);
    print "\n";
    
    if ($lf) {
      longmsg();
    }
    $prev_sc = $sc;

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
    print colored(['red'],"err\n");
  }
}

sub servmsg {
  return if (not exists $servmsgbuf{'ecc'});
  print "Service Message (errs ";
  if ($servmsgbuf{'errors'} =~ /1/) {
    print colored(['red'],$servmsgbuf{'errors'});
  } else {
    print colored(['green'],$servmsgbuf{'errors'});
  }
  
  print ") [[\n";
  @bytes = @{$servmsgbuf{'data'}};
  printf("  ECC: %02x\n",$servmsgbuf{'ecc'});
  printf("  TSEID: %02x\n",$servmsgbuf{'tseid'});
  printf("  Message Length: %d bytes\n",$servmsgbuf{'ml'});
  given ($servmsgbuf{'type'}) {
    when (0b0000) {
      print "  Channel Organization Table (COT)\n";
      print "    ServID  Scrambl  Avail\n";
      my $n = 3;
      while (defined $bytes[$n+1]) {
        $sid = ($bytes[$n] << 8) + ($bytes[$n+1] >> 2);
        last if ($sid == 0);
        $ca = ($bytes[$n+1] >> 1) & 1;
        $sa = ($bytes[$n+1]) & 1;
        printf("    %04x      [%s]     [%s]\n",$sid,($ca?"X":" "),($sa?"X":" "));
        $n += 2 + $ca;
      }
    }
    when (0b0001) {
      $ft = $bytes[3] >> 6;
      print "  Alt. Frequencies for Frame ".qw( A0 A1 B C )[$ft]."\n";
      $afnum = $bytes[3] & 0b111111;
      $tf = dec_af($bytes[4]);
      print "    Tuned Frequency: $tf MHz\n";
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
      $nname .= chr($bytes[10+$_] // 0) for (0..$nnl-1);
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
      while (defined $bytes[$n] && defined $bytes[$n+2] && $bytes[$n] != 0) {
        $ext = $bytes[$n] >> 7;
        if ($ext) {
          $sid = ( ($bytes[$n] & 0b1111111) << 7) + ($bytes[$n+1] >> 1);
        } else {
          $sid = $bytes[$n] & 0b1111111;
        }
        $cy = ($bytes[$n+2] >> 5) & 0b111;
        $dt = $bytes[$n+2] & 0b11111;
        @cycles = ([],
                   [0,2,4,6,8,10,12,14,16,18,20,22],
                   [0,3,6,9,12,15,18,21],
                   [0,4,8,12,16,20],
                   [0,6,12,18],
                   [0,8,16],
                   [0,12],
                   [0]);
        @every = ("","10th sec","15th sec","20th sec","30th sec","40th sec","min","2nd min");
        print "    Service ID ".sprintf("%04x",$sid)." in ";
        if ($cy == 0) {
          print "all frames";
        } else {
          @frames = @{$cycles[$cy]};
          $_ += $dt for (@frames);
          print "frames ".join(",",@frames). " (every $every[$cy])";
        }
        print "\n";
        $n = $n + 1 + $ext;
      }

    }
  }

  print "]]\n";
}

sub longmsg {
  my $dta = $lmsg{'data'};

  print "Long Message: [[";
  print "\n  errors: ";
  if ($lmsg{'errors'} =~ /1/) {
    print colored(['red'],$lmsg{'errors'}."\n");
  } else {
    print colored(['green'],$lmsg{'errors'}."\n");
  }
  my @hdr = unpack 'C*', $dta; #split(//,$dta);
  $ri = $hdr[0] >> 6;
  $ci = ($hdr[0] >> 4) & 0b11;
  $fl = ($hdr[0] >> 2) & 0b11;
  $ext = ($hdr[0] >> 1) & 1;
  $add = (($hdr[0] & 1) << 8) + $hdr[1];
  if ($ext) {
    #$extadd = $hdr[2] >> 3;
  }
  $com = ($hdr[2+$ext] >> 7) &1;
  $caf = ($hdr[2+$ext] >> 6) &1;
  $dlen = (($hdr[2+$ext] & 0b111111) << 2) + ($hdr[3+$ext] >> 6);

  #if (!$caf) {
    $crc = ($hdr[3+$ext] & 0b111111);
    #} 

  $calc_synd = crc6(substr($dta,0,4), pack("S>",$crc), 6);
  #my $synd = crc6();

  if ($calc_synd eq "00") {
    printf( "  ri $ri  ci $ci  f/l %02b  ext? $ext  add %03x  com? $com  caf? $caf".
            " dlen $dlen  L4crc %02x  synd $calc_synd\n",$fl,$add,$crc);
    $dta    = substr($dta,4,$dlen);
    $type   = ord(substr($dta,0,1)) >> 4;
    $crcf   = (ord(substr($dta,0,1)) >> 3) & 1;
    $comp   = (ord(substr($dta,0,1)) >> 2) & 1;
    $fragl5 = (ord(substr($dta,0,1)) >> 1) & 1;
    $fragsz = (ord(substr($dta,0,1)) >> 0) & 1 if ($fragl5 == 1);
    $packid = (ord(substr($dta,1,1)) >> 4) if ($fragl5 == 1);

    printf("  L5 hdr typ: %04b  crc? $crcf  compr? $comp  frag? $fragl5  ".
           ($fragl5 == 1 ? "fragsz? $fragsz  " : ""),$type);
    printf("packetid: %01x",$packid) if ($fragl5 == 1);
    print "\n";
    if ($fragl5 == 1) {
      print "  (fragmented, todo)\n";
    } else {
      $l5crc = substr($dta,-2);
      $dta = substr($dta,1,-2);
      print "  L5 data (".($lmsg{'errors'} =~ /1/ ? ":|" : "OK").") to ".sprintf("%03x",$add).": ";
      printsafe($dta);
      print "\n               ";
      printhex($dta);
      print "\n";
    }
  } else {
    print "  (invalid header - missed fragment?)\n";
  }
  print "]]\n";
  %lmsg = ();  
}

sub scramble_init {
  $scramble_ptr = 0;
  @mseq = qw( 1 0 1 0 1 1 1 1 1 0 1 0 1 0 1 0 1 0 0 0 0 0 0 1 0 1 0 0 1 0 1 0 1 1
              1 1 0 0 1 0 1 1 1 0 1 1 1 0 0 0 0 0 0 1 1 1 0 0 1 1 1 0 1 0 0 1 0 0
              1 1 1 1 0 1 0 1 1 1 0 1 0 1 0 0 0 1 0 0 1 0 0 0 0 1 1 0 0 1 1 1 0 0
              0 0 1 0 1 1 1 1 0 1 1 0 1 1 0 0 1 1 0 1 0 0 0 0 1 1 1 0 1 1 1 1 0 0
              0 0 1 1 1 1 1 1 1 1 1 0 0 0 0 0 1 1 1 1 0 1 1 1 1 1 0 0 0 1 0 1 1 1
              0 0 1 1 0 0 1 0 0 0 0 0 1 0 0 1 0 1 0 0 1 1 1 0 1 1 0 1 0 0 0 1 1 1
              1 0 0 1 1 1 1 1 0 0 1 1 0 1 1 0 0 0 1 0 1 0 1 0 0 1 0 0 0 1 1 1 0 0
              0 1 1 0 1 1 0 1 0 1 0 1 1 1 0 0 0 1 0 0 1 1 0 0 0 1 0 0 0 1 0 0 0 0
              0 0 0 0 1 0 0 0 0 1 0 0 0 1 1 0 0 0 0 1 0 0 1 1 1 0 0 1 );
}

sub dec_af {
  if (defined($_[0]) && $_[0] >= 1 && $_[0] <= 204) {
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
  my @chars = unpack 'C*', $_[0]; #split(//,$_[0]);
  for (@chars) {
    if ( ($_ > 31 && $_<127) || $_ > 160) {
      print encode("UTF-8",decode("iso-8859-1",chr($_)));
    } else {
      print ".";
    }
  }
}

sub printhex {
  my @chars = unpack 'C*', $_[0]; #split(//,$_[0]);
  for (@chars) {
    printf("%02x ",$_);
  }
}

# Layer2 CRC
# crc14(data, init)
sub crc14 {

  my $result = crc_general ($_[0], $_[1], 14, 0,
                            14,11,2,0);
  my $reshex = "";
  $reshex .= sprintf("%01x",eval("0b".substr($result,$_*4,4))) for (0..14/4);
  $reshex;

}

# Layer3, Layer4 LMCh header CRC
sub crc6 {

  my $result = crc_general($_[0], $_[1], 6, $_[2] // 0,
                           6, 4, 3, 0);
  
  my $reshex = "";
  $reshex .= sprintf("%01x",eval("0b".substr($result,$_*4,4))) for (0..6/4);
  $reshex;
}

# Layer5 CRC
sub crc16 {

  #TODO: bit inversion magic

  my $result = crc_general($_[0], $_[1], 16, $_[2] // 0,
                           16, 12, 5, 0);
  
  my $reshex = "";
  $reshex .= sprintf("%01x",eval("0b".substr($result,$_*4,4))) for (0..16/4);
  $reshex;
}


# Horizontal parity
# crc82(data, init)
sub crc82 {

  my $result = crc_general ($_[0], $_[1], 82, $_[2] // 2,
                           82, 77, 76, 71, 67, 66, 56, 52, 48, 40, 36, 34, 24, 22, 18, 10, 4, 0);
  
  my $reshex = "";
  $reshex .= sprintf("%01x",eval("0b".substr($result,$_*4,4))) for (1..82/4+1);
  $reshex;

}

# crc_general(data, init, len, clipbits, coeffs)
sub crc_general {
  my $input    = shift;
  my $init     = shift;
  my $len      = shift;
  my $clipbits = shift;
  my @coeffs   = @_;

  my $poly   = "0" x ($len+1);
  substr($poly,length($poly)-$_-1,1) = 1 for (@coeffs);
  my $data = unpack("B*",$input);
  substr($data,-$clipbits,$clipbits) = "" if ($clipbits > 0);
  $init = unpack("B*",$init);
  $data .= substr($init,-$len);
  for $a (0..length($data)-$len-1) {
    if (substr($data,$a,1) == 1) {
      for $b (0..$len) {
        substr($data,$a+$b,1) = (0+substr($data,$a+$b,1)) ^ (0+substr($poly,$b,1));
      }
    }
  }
  my $result = ("0" x (8-($len % 8))).substr($data,-$len);
  $result;
} 
