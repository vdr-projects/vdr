#!/usr/bin/perl

while (<>)
  {
    chomp;
    if ($_ && !(/^\#/))
      {
	($title, $subtitle, $description, $channel, $timeframe, $prio, $timer_title, $margin, $machine) = split (/\|/,$_);
	
	if ($timer_title) {
	  print "[$timer_title]\n";
	} elsif ($title) {
	  print "[$title]\n";
	} elsif ($subtitle) {
	  print "[$subtitle]\n";
	} elsif ($description) {
	  print "[$description]\n";
	} else {
	  die ("Illegal Format");
	}
	
	# Accept torecord only if it is for the current machine
	if ($title)
	  {
	    print "Title = $title\n";
	  }
	if ($subtitle)
	  {
	    print "Subtitle = $subtitle\n";
	  }
	if ($description)
	  {
	    print "Description = $description\n";
	  }
	if ($channel)
	  {
	    print "Channel = $channel\n";
	  }
	if ($timeframe)
	  {
	    print "Timeframe = $timeframe\n";
	  }
	if ($prio)
	  {
	    print "Prio = $prio\n";
	  }
	if ($timer_title)
	  {
	    print "Timertitle = $timer_title\n";
	  }
	if ($margin)
	  {
	    print "Margin = $margin\n";
	  }
	if ($machine)
	  {
	    print "Instance = $machine\n";
	  }
	print "\n";
      }
  }
