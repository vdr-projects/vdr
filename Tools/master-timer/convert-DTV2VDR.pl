#!/usr/bin/perl -w

use strict;

# The EPG-Entrys
my (%Entry, %channel, $mode);

# 0 = VDR -> DTV
# 1 = DTV -> VDR
$mode = 0;

read_channel_list();
if ($mode) {
  &read_dtv();
  &read_epgdata();
} else {
  &read_epgdata();
  &read_dtv();
}
&print_VDR();

sub read_epgdata {
  my ($channel, $duration, $title, $subtitle, $description, $time);
  open (FI,"epg.data") or die ("Can't open file \"epg.data\"\n");

  while (<FI>) {
    # Begin Channel
    if (/^C\s(\d+)\s+(.+)/) {
      $channel=$2;
      while (<FI>) {
	# End Channel
	if (/^c$/) {
	  last;
	}
	# Begin Timer
	elsif (/^E\s(\d+)\s+(\d+)\s+(\d+)$/) {
	  # Undef this Variables because it is possibel that not every timer uses this values
	  undef $duration;
	  undef $subtitle;
	  undef $description;

	  $time=$2;
	  $duration=$3;
	}
	# Title
	elsif (/^T\s(.*)/) {
	  $title=$1;
	}
	# Subtitle
	elsif (/^S\s(.*)/) {
	  $subtitle=$1;
	}
	# Description
	elsif (/^D\s(.*)/) {
	  $description=$1;
	}
	# End Timer
	elsif (/^e$/) {
	  if ($mode) {
	    # DTV -> VDR
	    $Entry{$channel}{$time}{subtitle}=$subtitle if ($subtitle);
	    if ($description) {
	      if ($Entry{$channel}{$time}{description}) {
		$Entry{$channel}{$time}{description} = "DTV: '$Entry{$channel}{$time}{description}' VDR: '$description'";
	      } else {
		$Entry{$channel}{$time}{description} = "DTV: '' VDR: '$description'";
	      }
	    }
	  } else {
	    # VDR -> DTV
	    $Entry{$channel}{$time}{title}=$title;
	    $Entry{$channel}{$time}{duration}=$duration;
	    $Entry{$channel}{$time}{subtitle}=$subtitle if ($subtitle);
	    $Entry{$channel}{$time}{description}=$description if ($description);
	  }
	}
      }
    }
  }
  close (FI);
}

sub read_dtv {
  my ($channel, $time, $duration, $title, $category, $subtitle, $description);
  open (FI,$ARGV[0]) or die "Can't open DTV-File";

  while (<FI>) {
    chomp;
    ($channel, $time, $duration, $title, $category, $subtitle, $description) = split (/\|/);
    if (!$channel{$channel}) {
      next;
    }
    $channel = $channel{$channel};
    if ($mode) {
      # DTV -> VDR
      if (!$subtitle && $description =~ /^\"(.*?)\"\:\s(.*)/) {
	$Entry{$channel}{$time}{subtitle} = $1;
	$description = $2;
      }
      $Entry{$channel}{$time}{title} = $title;
      $Entry{$channel}{$time}{duration} = $duration;
      $Entry{$channel}{$time}{subtitle} = $subtitle if ($subtitle);
      $Entry{$channel}{$time}{category} = $category if ($category);
      $Entry{$channel}{$time}{description} = $description if ($description);
    } else {
      # VDR -> DTV
      $Entry{$channel}{$time}{category} = $category if ($category);
      if ($description) {
	if (!$Entry{$channel}{$time}{subtitle} && $description =~ /^\"(.*?)\"\:\s(.*)/) {
	  $Entry{$channel}{$time}{subtitle} = $1;
	  $description = $2;
	}
	if ($Entry{$channel}{$time}{description}) {
	  $Entry{$channel}{$time}{description} = "DTV: '$description' VDR: '$Entry{$channel}{$time}{description}'";
	} else {
	  $Entry{$channel}{$time}{description} = "DTV: '$description' VDR: ''";
	}
      }
    }
  }
  close (FI);
}

sub read_channel_list {
  my ($old, $new);
  open (FI,"$ENV{HOME}/.master-timer/convert-channel-list") or die ("Can't read channel-List");
  while (<FI>) {
    chomp;
    ($old, $new) = split (/\|/);
    $channel{$old} = $new;
  }
  close (FI);
}

sub print_VDR() {
  my ($channel, $title, $time);
  foreach $channel (sort keys %Entry) {
    print "C 1 $channel\n";
    foreach $time (sort keys %{%Entry->{$channel}}) {
      if ($Entry{$channel}{$time}{duration}) {
	print "E 1 $time $Entry{$channel}{$time}{duration}\n";
	print "K $Entry{$channel}{$time}{category}\n" if ($Entry{$channel}{$time}{category});
	print "T $Entry{$channel}{$time}{title}\n";
	print "S $Entry{$channel}{$time}{subtitle}\n" if ($Entry{$channel}{$time}{subtitle});
	print "D $Entry{$channel}{$time}{description}\n" if ($Entry{$channel}{$time}{description});
	print "e\n";
      }
    }
    print "c\n";
  }
}
