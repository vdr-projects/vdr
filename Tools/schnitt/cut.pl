#!/usr/bin/perl

chdir ($ARGV[0]) if ($ARGV[0]);

$read = $size = 1024*1024;

$filenum = "1";
$count = 0;

$fi = sprintf ("part%d",$filenum);
open (FI,">$fi");

while ($read == $size)
  {
    if ($count < 660*1024*1024)
      {
	$read = read (STDIN,$data,$size);
	print FI $data;
	$count += $size;
	$a = $count /1024/1024;
	if ($a % 10 == 0) {
          print stderr "File: $filenum Size: ${a}MB\n";
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
