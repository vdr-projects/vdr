#!/usr/bin/perl

open (FI,$ARGV[0]) or die "Kann Input-Datei nicht oeffnen";
$count = 1;

while (<FI>)
  {
    chomp;
    $char = sprintf ("%c",$count + 96);
    print "Cutting from/to $_ into /x2/clips/$char\n";
    system ("/usr/local/bin/my/schnitt2.pl $_ > /x2/clips/$char");
    $count++;
  }
