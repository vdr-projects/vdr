#!/usr/bin/perl

require "/usr/local/bin/my/schnittcommon.pli";

if (!open (INDEX,"index.vdr"))
  {
    exit 1;
  }
$index = $ARGV[0];
&nextI;
$offset1 = $offset;
&readnext;
$off = $offset - $offset1;
close (FI);
$fi = sprintf ("%03d.vdr",$file);
open (FI,$fi);
open (FO,">bild");
sysseek (FI,$offset1,0);
sysread (FI,$temp,200000);
syswrite (FO,$temp,200000);
close (FI);
close (FO);

`/usr/local/bin/pvademux.old /x2/temp bild`;
#`/usr/local/bin/pes2av_pes bild | /usr/local/bin/pvademux /x2/temp bild`;
print "$index\n";
