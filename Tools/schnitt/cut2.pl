#!/usr/bin/perl

$titel = $ARGV[0];

chdir ("/x2/temp");

@files=<teil*.mpg>;
$cd = 1;
mkdir "/x2/temp/$cd";
open (FF,">/x2/temp/$cd/$titel\ CD\ $cd");
close (FF);

foreach $file (@files)
  {
    $size = -s $file;
    $total += $size;
    if ($total <= 660*1024*1024)
      {
	print "Moving $file\n";
	system ("mv /x2/temp/$file /x2/temp/$cd/$file");
      }
    else
      {
	print "Splitting $file\n";
	$file =~ s/\.mpg$//;
	$total -= $size;
	$size = (660*1024*1024) - $total;
	$cd = `cut3.pl /x2/temp $cd $file $size \'$titel\' < $file.mpg`;
	chomp $cd;
        $total = 0;
	@files2=</x2/temp/$cd/teil*>;
	foreach $file2 (@files2)
	  {
	    $total += -s $file2;
	  }
        print "CD: $cd Total $total\n";
	unlink "$file.mpg";
      }
  }
