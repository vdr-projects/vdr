#!/usr/bin/perl

open (FI,"cut") or die "Kann Cut-Datei nicht oeffnen\n";

outer: while (<FI>)
  {
    chomp;
    if (! ($_ > 1 || $_ eq "0"))
      {
	open (FO,">cut2");
	print FO "$_\n";
	while (<FI>)
	  {
	    chomp;
	    if ($_ > 1 || $_ eq "0")
	      {
		print FO "$_\n";
	      }
	    else
	      {
		system ("cutt");
		redo outer;
	      }
	  }
      }
  }
if ( -f "cut2")
  {
    system ("cutt");
    unlink "cut2";
  }
