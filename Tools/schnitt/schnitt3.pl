#!/usr/bin/perl

require "/usr/local/bin/my/schnittcommon.pli";

open (INDEX,"index.vdr");
$index = $ARGV[0];
&nextI;

$oldindex = $index;
$tempindex = $index;

$add = -1;

$fi = sprintf ("%03d.vdr",$file);
open (FI2,$fi);
open (FO,">test");
sysseek (FI2,$offset,0);
sysread (FI2,$temp,3000000);
syswrite (FO,$temp,3000000);
close (FI2);
close (FO);
`/usr/local/bin/pvademux.old . test`;
if ( -s "test.mp2")
  {
    `rm test*`;
    print "$index\n";
    exit 0;
  }

while (1)
  {
    if ($index == 0)
      {
        $add = 1;
      }
    if ($add = -1)
      {
       $index--;
       &prevI;
      }
    else
      {
        nextI;
      }
    $fi = sprintf ("%03d.vdr",$file);
    open (FI2,$fi);
    open (FO,">test");
    sysseek (FI2,$offset,0);
    sysread (FI2,$temp,3000000);
    syswrite (FO,$temp,3000000);
    close (FI2);
    close (FO);
    `/usr/local/bin/pvademux.old . test`;
    if ( -s "test.mp2")
      {
        `rm test*`;
        if ($index < 0)
          {
            $index *= -1;
          }
        print "$index\n";
        exit 0;
      }
  }
