#!/usr/bin/perl -w

$dir = "/home/ms/.master-timer";

open (FI,"$dir/done") or die "Can't open \"done\"\n";
while (<FI>)
  {
    chomp;
    if ($_)
      {
        ($title,$subtitle) = split (/\|/,$_,2);
        $Done{$title}{$subtitle}=1;
      }
  }
close (FI);

&traverse('/video');

if ($hit)
  {
    rename ("$dir/done","$dir/done.bak");
    open (FO,">$dir/done");
    foreach $title (sort keys %Done)
      {
	foreach $subtitle (sort keys %{%Done->{$title}})
	  {
	    print FO "$title\|$subtitle\n";
	  }
      }
  }

sub traverse
  {
    local($dir) = shift;
    local($path);
    unless (opendir(DIR, $dir))
      {
	warn "Can't open $dir\n";
	closedir(DIR);
	return;
      }
    foreach (readdir(DIR))
      {
	next if $_ eq '.' || $_ eq '..';
	$path = "$dir/$_";
	if (-d $path) 	# a directory
	  {
	    &traverse($path);
	}
	if ($_ eq "summary.vdr")
	  {
	    open (FI,"$path") or die "Can't open \"$path\"\n";
	    @lines = <FI>;
	    close (FI);
	    if ($lines[0] =~ /^Title\:\s\"(.*)\"/)
	      {
		@titles = split (/\~/,$1);
		if ($lines[2] && $lines[2] =~ /^Subtitle\:\s\"(.*)\"/)
		  {
		    @subtitles = split (/\~/,$1);
		    foreach $num (0 .. $#titles)
		      {
			if ($titles[$num] && $subtitles[$num])
			  {
			    if (!$Done{$titles[$num]}{$subtitles[$num]})
			      {
				$Done{$titles[$num]}{$subtitles[$num]}=1;
				$hit = 1;
			      }
			  }
		      }
		  }
	      }
	  }
      }
    closedir(DIR);
  }


