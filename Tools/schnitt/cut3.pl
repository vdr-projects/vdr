#!/usr/bin/perl

$read = $size = 1024*1024;

$dir = $ARGV[0];
$subdir = $ARGV[1];
$teil = $ARGV[2];
$count1 = $ARGV[3];
$title = $ARGV[4];

$filenum = "1";
$count = 0;

open (FI,">$dir/$subdir/$teil.$filenum.mpg");

while ($read == $size)
  {
    if (($filenum == 1 && $count < $count1) || ($filenum > 1 && $count < 660*1024*1024))
      {
	$read = read (STDIN,$data,$size);
	print FI $data;
	$count += $size;
	$a = $count /1024/1024;
      }
    else
      {
	close (FI);
	$filenum++;
	$subdir++;
        mkdir ("$dir/$subdir");
	open (FF,">$dir/$subdir/$title\ CD\ $subdir");
	close (FF);
	open (FI,">$dir/$subdir/$teil.$filenum.mpg");
	$count = 0;
      }
  }

close FI;

print "$subdir\n";
