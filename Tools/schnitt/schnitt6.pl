#!/usr/bin/perl

open (FI,"a");

while (<FI>)
{
  open (SCH,"/usr/local/bin/my/schnitt5.pl $_|");
  $files = <SCH>;
  chomp $files;
  ($a,$b) = split (/\s/,$files);
  $files[$a] = 1;
  $files[$b] = 1;
  close (SCH);
}

while (<0*.vdr>)
{
  $_ =~ /\d(\d\d)\.vdr/;
  if ($files[$1])
  {
    print "Keeping $1\n";
  }
  else
  {
    print "Deleting $_\n";
    unlink $_;
  }
}

close (FI);
