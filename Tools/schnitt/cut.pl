#!/usr/bin/perl -w

use strict;

my $maxsize = 660 * 1024 * 1024;

my $read = 1024*1024;
my $size = 1024*1024;

my $filenum = "1";
my $count = 0;

my ($fi,$data);

$fi = sprintf ("part%d",$filenum);
open (FI,">$fi");

while ($read == $size)
  {
    if ($count < $maxsize)
      {
	$read = read (STDIN,$data,$size);
	print FI $data;
	$count += $size;
	$a = $count /1024/1024;
	if ($a % 10 == 0) {
          print STDERR "File: $filenum Size: ${a}MB\n";
        }
      }
    else
      {
	close (FI);
	$filenum++;
	$fi = sprintf ("part%d",$filenum);
	open (FI,">$fi");
	$count = 0;
      }
  }

close FI;
