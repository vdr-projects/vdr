#!/usr/bin/perl -w

use strict;
# For the TCP-Connection to VDR
use Socket;
# For converting the Timers, read from VDR, back to Unix-Timestamps
use Time::Local;
# For parsing the command line
use Getopt::Std;

# Debugmode
# You have to add the following numbers to build the debug-var
# 1     : Dump the "torecord"
# 2     : Dump all timers
# 4     : Show when a timer will be deleted
# 8     : Dump the "Done" REs
# 16    : Verbose Config-Reading
# 32    : Dump Program Variable
# 64    : Excessive deepblack/torecord debuging
my $debug = 6;

# The Supervariable Program
# %Program{$title}{$channel}{$time}{duration}
#                                  {subtitle}
#                                  {description}

# The Supervariable Timer
# %Timer{$time}{$channel}{$title}{duration}
#                                {subtitle}
#                                {description}
#                                {prio}
#                                {lifetime}
#                                {real_title}
#                                {VDR} (Already programmed)
# The Value of VDR is ">0" for the position in the Timer-List or "R" for a "Repeating" Timer.
# A Value of >1.000.000 is a Master Timer-Timer which is already programmed into VDR

# The Supervariable torecord/deepblack
# $torecord{timercount}
#          {titleRE}
#          {subtitleRE}
#          {descriptionRE}
#          {title}[COUNT]
#          {subtitle}[COUNT]
#          {description}[COUNT]
#          {timeframe}[COUNT]
#          {blackchannel}[COUNT] or {channel}[COUNT]
#          {weekday}[COUNT]
#          {minlength}[COUNT]
#          {maxlength}[COUNT]
#          {prio}[COUNT]
#          {timertitle}[COUNT]
#          {marginstart}[COUNT]
#          {marginstop}[COUNT]
#          {instance}[COUNT]

# Variable-Definition
my (%Program, @channels, %channels, %Timer);

# Which Subtitles are Movies
my ($subtitle_movie);
my ($test_subtitle_movie);

# Blacklist
my (%deepblack);

# What is already recorded/Should not be recorded
my ($title_done, $subtitle_done);

# What to record
my (%torecord);

# The Commandline
my (%Opts);

# Default Priority for Timers (Config: defaultprio)
my $default_prio = 50;

# How many DVB-S cards are there (Config: DVBCards)
my $DVB_cards = 1;

# How many seconds to substract from the time and to add to the duration
my $marginstart = 60*10; # Config: Marginstart
my $marginstop = 60*10; # Config: Marginstop

# Shall Timers, on the same channel, be joined if they overlap
my $jointimers = 0;

# Hostname/IP of DVB-Computer and the Port of VDR
my @Dest = ("localhost:2001"); # Config: Dest

# Which VDR-Instance shall be used
my $currentVDR = 1;

# Where are the Config-Files
my $configdir = "$ENV{HOME}/.master-timer";

# Should the description be transfered to VDR?
my $Description = 0;

# Working-Variables
my ($title, $duration, $subtitle, $channel, $time, $description, $category, $hit);
my (@time, @date);

END {
  &closesocket();
}

&init();
&dumpdone() if ($debug & 8);
&dumptorecord("torecord") if ($debug & 1);
&dumptorecord("deepblack") if ($debug & 1);
print "Subtitle-Movie \"$subtitle_movie\"\n" if($debug & 1);
# If we only have to dump the running series then exit after dumping them
if ($Opts{s}) {
  &dumpepgdata;
  exit 0;
}
&processdone();
&fetchVDRTimers();
&process_torecord();
print "Timers before joining\n" if ($debug & 2 && $jointimers);
&dumptimers() if ($debug & 2);

if ($jointimers) {
  &jointimers();
  print "Timers after joining\n" if ($debug & 2);
  &dumptimers() if ($debug & 2);
}

&dumpepgdata if ($debug & 32);

&printtimers();
&transfertimers();

#
# End of Program
#

#
# Subfunctions
#

sub dumpdone() {
  print "Start Done-dump\n";
  print "Titledone: \"$title_done\"\n";
  print "Subtitledone \"$subtitle_done\"\n";
  print "End Done-dump\n";
}

sub dumpepgdata () {
  print "Start EPG-Dump\n";
  foreach $title (sort keys %Program) {
    foreach $channel (sort keys %{%Program->{$title}}) {
      foreach $time (sort {$a <=> $b} keys %{%Program->{$title}->{$channel}}) {
	print "Title: \"$title\" ";
	if (!$Opts{s}) {
	  print "Subtitle: \"$Program{$title}{$channel}{$time}{subtitle}\" " if ($Program{$title}{$channel}{$time}{subtitle});
	  print "Time: \"$time\"";
	}
	print "Channel: \"$channel\"";
	print "\n";
	if ($Opts{s}) {
	  last;
	}
      }
    }
  }
  print "End EPG-Dump\n";
}


sub dumptorecord() {
  my ($context) = shift;
  my ($rContext);

  if ($context eq "torecord") {
    $rContext = \%torecord;
  } elsif ($context eq "deepblack") {
    $rContext = \%deepblack;
  } else {
    die ("Illegal Context");
  }

  print "Start $context-dump\n";
  print "Regex-Title: $$rContext{titleRE}\n";
  print "Regex-Subtitle: $$rContext{subtitleRE}\n";
  print "Regex-Description: $$rContext{descriptionRE}\n";
  foreach my $num (0 .. $$rContext{timercount}) {
    print "Entry Number $num: ";

    print "Title: \"$$rContext{title}[$num]\" " if ($$rContext{title}[$num]);
    print "Title: \"\" " unless ($$rContext{title}[$num]);

    print "Subtitle: \"$$rContext{subtitle}[$num]\" "if ($$rContext{subtitle}[$num]);
    print "Subtitle: \"\" " unless ($$rContext{subtitle}[$num]);
	
    print "Description: \"$$rContext{description}[$num]\" " if ($$rContext{description}[$num]);
    print "Description: \"\" " unless ($$rContext{description}[$num]);
	
    print "Category: \"$$rContext{category}[$num]\" " if ($$rContext{category}[$num]);
    print "Category: \"\" " unless ($$rContext{category}[$num]);
	
    print "Timeframe: \"$$rContext{timeframe}[$num]\" " if ($$rContext{timeframe}[$num]);
    print "Timeframe: \"\" " unless ($$rContext{timeframe}[$num]);
	
    print "Weekday: \"$$rContext{weekday}[$num]\" " if ($$rContext{weekday}[$num]);
    print "Weekday: \"\" " unless ($$rContext{weekday}[$num]);
	
    print "Channel: \"$$rContext{channel}[$num]\" " if ($$rContext{channel}[$num]);
    print "Channel: \"\" " unless ($$rContext{channel}[$num]);
	
    print "Blackchannel: \"$$rContext{blackchannel}[$num]\" " if ($$rContext{blackchannel}[$num]);
    print "Blackchannel: \"\" " unless ($$rContext{blackchannel}[$num]);
	
    print "Prio: \"$$rContext{prio}[$num]\" " if ($$rContext{prio}[$num]);
    print "Prio: \"\" " unless ($$rContext{prio}[$num]);
	
    print "Timertitle: \"$$rContext{timertitle}[$num]\" " if ($$rContext{timertitle}[$num]);
    print "Timertitle: \"\" " unless ($$rContext{timertitle}[$num]);

    print "Marginstart: \"$$rContext{marginstart}[$num]\" " if ($$rContext{marginstart}[$num]);
    print "Marginstart: \"\" " unless ($$rContext{marginstart}[$num]);

    print "Marginstop: \"$$rContext{marginstop}[$num]\" " if ($$rContext{marginstop}[$num]);
    print "Marginstop: \"\" " unless ($$rContext{marginstop}[$num]);

    print "Minlength: \"$$rContext{minlength}[$num]\" " if ($$rContext{minlength}[$num]);
    print "Minlength: \"\" " unless ($$rContext{minlength}[$num]);

    print "Maxlength: \"$$rContext{maxlength}[$num]\" " if ($$rContext{maxlength}[$num]);
    print "Maxlength: \"\" " unless ($$rContext{maxlength}[$num]);

    print "Instance: \"$$rContext{instance}[$num]\" " if ($$rContext{instance}[$num]);
    print "Instance: \"\" " unless ($$rContext{instance}[$num]);

    print "\n";
  }
  print "End $context-dump\n";
}

sub dumptimers() {
  print "Start Timers-dump\n";
  foreach $time (sort {$a <=> $b} keys %Timer) {
    foreach $channel (sort keys %{%Timer->{$time}}) {
      foreach $title (sort keys %{%Timer->{$time}->{$channel}}) {
	my ($prio, $lifetime, @time, @date, @time2);
	my ($realtitle);
	@time = &GetTime ($time);
	@date = &GetDay ($time);
	@time2 = &GetTime ($time + $Timer{$time}{$channel}{$title}{duration});
	$subtitle = $Timer{$time}{$channel}{$title}{subtitle};
	$prio = $Timer{$time}{$channel}{$title}{prio};
	$lifetime = $Timer{$time}{$channel}{$title}{lifetime};
	$realtitle = $Timer{$time}{$channel}{$title}{real_title};
	print "2:$channels{$channel}{number}:$date[1]:$time[0]$time[1]:$time2[0]$time2[1]:$prio:$lifetime:$title:Title: \"$realtitle\"||Subtitle: \"$subtitle\":$Timer{$time}{$channel}{$title}{VDR}\n";
      }
    }
  }
  print "End Timers-dump\n";
}

sub printtimers() {
  foreach $time (sort {$a <=> $b} keys %Timer) {
    foreach $channel (sort keys %{%Timer->{$time}}) {
      foreach $title (sort keys %{%Timer->{$time}->{$channel}}) {
	my ($prio, $lifetime, @time, @date, @time2);
	if ($Timer{$time}{$channel}{$title}{VDR} eq 0) {
	  my ($realtitle);
	  @time = &GetTime ($time);
	  @date = &GetDay ($time);
	  @time2 = &GetTime ($time + $Timer{$time}{$channel}{$title}{duration});
	  $subtitle = $Timer{$time}{$channel}{$title}{subtitle};
	  $prio = $Timer{$time}{$channel}{$title}{prio};
	  $lifetime = $Timer{$time}{$channel}{$title}{lifetime};
	  $realtitle = $Timer{$time}{$channel}{$title}{real_title};

	  print "2:$channels{$channel}{number}:$date[1]:$time[0]$time[1]:$time2[0]$time2[1]:$prio:$lifetime:$title:Title: \"$realtitle\"||Subtitle: \"$subtitle\"\n";
	}
      }
    }
  }
}

sub transfertimers() {
  foreach $time (sort {$a <=> $b} keys %Timer) {
    foreach $channel (sort keys %{%Timer->{$time}}) {
      foreach $title (sort keys %{%Timer->{$time}->{$channel}}) {
	my ($prio, $lifetime, $description, @time, @date, @time2, $realtitle, $result);
	if ($Timer{$time}{$channel}{$title}{VDR} eq 0) {
	  @time = &GetTime ($time);
	  @date = &GetDay ($time);
	  @time2 = &GetTime ($time + $Timer{$time}{$channel}{$title}{duration});
	  $subtitle = $Timer{$time}{$channel}{$title}{subtitle};
	  $prio = $Timer{$time}{$channel}{$title}{prio};
	  $lifetime = $Timer{$time}{$channel}{$title}{lifetime};
	  if ($Description) {
	    $description = "||Description :\"$Timer{$time}{$channel}{$title}{description}\"";
	  } else {
	    $description = "";
	  }
	  $realtitle = $Timer{$time}{$channel}{$title}{real_title};

	  ($result) = GetSend ("newt 2:$channels{$channel}{number}:$date[1]:$time[0]$time[1]:$time2[0]$time2[1]:$prio:$lifetime:$title:Title: \"$realtitle\"||Subtitle: \"$subtitle\"$description");
	  print "Timer: $result" if ($debug & 2);
	}
      }
    }
  }
}

# Convert the Unix-Time-Stamp into "month" and "Day of month"
sub GetDay {
  my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(shift);
  $mon++;
  $mon = sprintf ("%02i",$mon);
  $mday = sprintf ("%02i",$mday);
  return ($mon, $mday);
}
# Convert the Unix-Time-Stramp into Weekday
sub GetWDay {
  my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(shift);
  return ($wday);
}

# Convert the Unix-Time-Stramp into "hour" and "minute"
sub GetTime {
  my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(shift);
  $hour = sprintf ("%02i",$hour);
  $min = sprintf ("%02i",$min);
  return ($hour, $min);
}

# Workaround some EPG-Bugs
sub correct_epg_data {
  if ($subtitle) {
    # For Pro-7. Remove $title from $subtitle
    $subtitle =~ s/\Q$title\E\s\/\s//;
	
    # For VOX & VIVA. The Format it '"<Subtitle>". <Description>'
    if ($subtitle =~ /^\"(.*?)\"\.\s(.*)/) {
      # Lets see if there are Channels that where the VOX/VIVA scheme matches, but also have a description
      if ($description) {
	my $one = $1;
	my $two = $2;
	if ($description =~ /^DTV\:\s\'(.*)\' VDR:\s\'\'$/) {
	  $description = "DTV: '$1' VDR: '$two'";
	  $subtitle = $one;
	} else {
	  die ("Title: \"$title\" Channel: \"$channel\" Subtitle: \"$subtitle\"\nDescription: \"$description\"\n");
	}
      }
      $subtitle = $1;
      $description = $2;
    }
    elsif ($channel eq "VIVA") {
      if ($subtitle =~ /^\s(.*)/) {
	$subtitle = "";
	$description = $1;
      }
    }
  }

  # Workaround for the broken PRO-7/Kabel-1 EPG-Date. If Time is between 00.00 and 05.00 the time is shifted forward by a day
  if ($channel eq "Pro-7" || $channel eq "Kabel-1") {
    my (@time);
    @time = GetTime ($time);
    if ($time[0] >= 0 && ($time[0] <= 4 || ($time[0] == 5 && $time[1] == 0))) {
      $time += 24*60*60;
    }
  }
}

# Add a Recording into the "to record"-List
sub addtimer {
  my ($title, $realtitle, $subtitle, $channel, $time, $duration, $prio, $lifetime, $description, $VDR, $time2, $title2, $channel2, $marginstart, $marginstop);
  ($title, $realtitle, $subtitle, $description, $channel, $time, $duration, $prio, $lifetime, $VDR, $marginstart, $marginstop) = @_;
#  print "Title: \"$title\" Realtitle: \"$realtitle\" Subtitle: \"$subtitle\" Channel: \"$channel\" Time: \"$time\" Duration: \"$duration\" Prio: \"$prio\" VDR: \"$VDR\"\n";

  foreach $time2 (sort keys %Timer) {
    foreach $title2 (sort keys %{%Timer->{$time2}->{$channel}}) {
      my ($ctime, $ctime2);
      $ctime = $time2;
      $ctime2 = $time2 + $Timer{$time2}{$channel}{$title2}{duration};

      if (($time >= $ctime) && ($time <= $ctime2)) {
	return;
      }
    }
  }

  $time -= $marginstart;
  $duration += $marginstart + $marginstop;
  $Timer{$time}{$channel}{$title}{duration}=$duration;
  $Timer{$time}{$channel}{$title}{subtitle}=$subtitle;
  $Timer{$time}{$channel}{$title}{description}=$description;
  $Timer{$time}{$channel}{$title}{prio}=$prio;
  $Timer{$time}{$channel}{$title}{lifetime}=$lifetime;
  $Timer{$time}{$channel}{$title}{VDR}=$VDR;
  $Timer{$time}{$channel}{$title}{real_title}=$realtitle;
}

sub deltimer() {
  my ($time, $channel, $title, $delete_from_VDR);
  ($time, $channel, $title, $delete_from_VDR) = @_;

#  if ($delete_from_VDR) {
#    if ($Timer{$time}{$channel}{$title}{VDR}) {
#      if ($Timer{$time}{$channel}{$title}{VDR} =~ s/ ^R/) {
#	print "Error: A Repeating-Timer can't be deleted from VDR: \"$title\"\n";
#      }
#      elsif ($Timer{$time}{$channel}{$title}{VDR} < 1000000) {
#	print "A User-Programmed Timer has been deleted from VDR: \"$title\"\n";
#      }
#      else {
#	
#      }
#    }
#  }

  delete $Timer{$time}{$channel}{$title}{duration};
  delete $Timer{$time}{$channel}{$title}{subtitle};
  delete $Timer{$time}{$channel}{$title}{prio};
  delete $Timer{$time}{$channel}{$title}{VDR};
  delete $Timer{$time}{$channel}{$title}{real_title};
  delete $Timer{$time}{$channel}{$title};
  delete $Timer{$time}{$channel} if (keys %{ $Timer{$time}{$channel} } == 1);
  delete $Timer{$time} if (keys %{ $Timer{$time} } == 1);
}

sub delprogram() {
  my ($title, $channel, $time);
  ($title, $channel, $time) = @_;

  delete $Program{$title}{$channel}{$time};
  delete $Program{$title}{$channel} if (keys %{ $Program{$title}{$channel} } == 1);
  delete $Program{$title} if (keys %{ $Program{$title} } == 1);
}

sub jointimers {
  #
  # FIXME: 2 Timers on the same channel will always be joined.
  # It should be checked if there is another DVB-Card available.
  #
  # FIXME2: When one timer is already programmed in VDR, delete that timer in VDR.
  my ($running, $counter, @times, $channel, $title, $channel2, $title2);
  $running = 1;
 outer: while ($running) {
    $counter = 0;
    @times = sort {$a <=> $b} keys %Timer;

    # We only need to check till the second last timer. The last one can't have a overlapping one.
    while ($counter < $#times) {
      foreach $channel (sort keys %{%Timer->{$times[$counter]}}) {
	foreach $title (sort keys %{%Timer->{$times[$counter]}->{$channel}}) {
	  if ($times[$counter + 1] < ($times[$counter] + $Timer{$times[$counter]}{$channel}{$title}{duration})) {
	    foreach $channel2 (sort keys %{%Timer->{$times[$counter + 1]}}) {
	      foreach $title2 (sort keys %{%Timer->{$times[$counter + 1]}->{$channel}}) {
		if ($channel eq $channel2) {
		  my ($duration, $subtitle, $description, $prio, $lifetime, $realtitle, $duration2, $subtitle2, $description2, $prio2, $lifetime2, $realtitle2);
		  # Values from Lower-Timer
		  $duration = $Timer{$times[$counter]}{$channel}{$title}{duration};
		  $subtitle = $Timer{$times[$counter]}{$channel}{$title}{subtitle};
		  $description = $Timer{$times[$counter]}{$channel}{$title}{description};
		  $prio = $Timer{$times[$counter]}{$channel}{$title}{prio};
		  $lifetime = $Timer{$times[$counter]}{$channel}{$title}{lifetime};
		  $realtitle = $Timer{$times[$counter]}{$channel}{$title}{real_title};

		  # Values from Higher-Timer
		  $duration2 = $Timer{$times[$counter + 1]}{$channel2}{$title2}{duration};
		  $subtitle2 = $Timer{$times[$counter + 1]}{$channel2}{$title2}{subtitle};
		  $description2 = $Timer{$times[$counter + 1]}{$channel2}{$title2}{description};
		  $prio2 = $Timer{$times[$counter + 1]}{$channel2}{$title2}{prio};
		  $lifetime2 = $Timer{$times[$counter + 1]}{$channel2}{$title2}{lifetime};
		  $realtitle2 = $Timer{$times[$counter + 1]}{$channel2}{$title2}{real_title};

		  # Use the Higher Priority/Lifetime for the new Timer
		  $prio = ($prio > $prio2) ? $prio : $prio2;
		  $lifetime = ($lifetime > $lifetime2) ? $lifetime : $lifetime2;

		  # Delete the two "Obsolet" Timers
		  &deltimer ($times[$counter], $channel, $title);
		  &deltimer ($times[$counter + 1], $channel2, $title2);

		  # And set the new one
		  &addtimer ("$title + $title2", "$realtitle\~$realtitle2", "$subtitle\~$subtitle2", "$description\~$description2", $channel, $times[$counter], $duration2 + ($times[$counter + 1 ] - $times[$counter]),$prio,$lifetime,0,0,0);

		  # Now a Value is "missing", so we will redo the whole thing. (This will do three-times JOIN correct)
		  redo outer;
		}
	      }
	    }
	  }
	}
      }
      $counter++;
    }
    undef $running;
  }
}

sub process_torecord {
  my ($subtitle, $description, $prio, $lifetime, $timertitle, $counter);
  foreach $title (sort keys %Program) {
    foreach $channel (sort keys %{%Program->{$title}}) {
      foreach $time (sort {$a <=> $b} keys %{%Program->{$title}->{$channel}}) {

	$counter = &testtimer("torecord", $title, $channel, $time);
	if ($counter ne "Nothing") {

	  # What Priority
	  if ($torecord{prio}[$counter]) {
	    $prio = $torecord{prio}[$counter];
	  }
	  else {
	    $prio = 50;
	  }

	  # What Lifetime
	  if ($torecord{lifetime}[$counter]) {
	    $lifetime = $torecord{lifetime}[$counter];
	  }
	  else {
	    $lifetime = 50;
	  }

	  # What Title to use for the timer
	  if ($torecord{timertitle}[$counter]) {
	    $timertitle = $torecord{timertitle}[$counter]
	  }
	  elsif ($torecord{title}[$counter]) {
	    $timertitle = $torecord{title}[$counter]
	  }
	  else {
	    $timertitle = $title;
	  }

	  # What subtitle to use
	  if ($Program{$title}{$channel}{$time}{subtitle}) {
	    $subtitle = $Program{$title}{$channel}{$time}{subtitle};
	  }
	  else {
	    $subtitle = "";
	  }

	  # What Description to use
	  if ($Program{$title}{$channel}{$time}{description}) {
	    $description = $Program{$title}{$channel}{$time}{description};
	  }
	  else {
	    $description = "";
	  }

	  &addtimer ($timertitle,$title,$subtitle,$description,$channel,$time,$Program{$title}{$channel}{$time}{duration},$prio,$lifetime,0,$torecord{marginstart}[$counter],$torecord{marginstop}[$counter]);
	}
      }
    }
  }
}

# Test if a torecord/deepblack Entry matches the current EPG-Data-Field
sub testtimer {
  my ($context) = shift;
  my ($title) = shift;
  my ($channel) = shift;
  my ($time) = shift;
  my ($counter, $rContext);

  if ($context eq "torecord") {
    $rContext = \%torecord;
  } elsif ($context eq "deepblack") {
    $rContext = \%deepblack;
  } else {
    die ("Illegal Context");
  }

  if ($debug & 64) {
    print "\n";
    print "Context: \"$context\"\nTitle: \"$title\"\n";
    print "Subtitle: \"$Program{$title}{$channel}{$time}{subtitle}\"\n" if ($Program{$title}{$channel}{$time}{subtitle});
    print "Description \"$Program{$title}{$channel}{$time}{description}\"\n" if ($Program{$title}{$channel}{$time}{description});
    print "Category \"$Program{$title}{$channel}{$time}{category}\"\n" if ($Program{$title}{$channel}{$time}{category});
    print "Channel: $channel\n";
    print "Time: $time\n";
    print "Duration: $Program{$title}{$channel}{$time}{duration}\n";
  }

  # First look if any of the Title/Subtitle/Description REs match
  if ($title =~ /$$rContext{titleRE}/i) {
    print "Title hit\n" if ($debug & 64);
  }
  elsif ($Program{$title}{$channel}{$time}{subtitle} && $Program{$title}{$channel}{$time}{subtitle} =~ /$$rContext{subtitleRE}/i) {
    print "SubTitle hit\n" if ($debug & 64);
  }elsif ($Program{$title}{$channel}{$time}{subtitle} && $test_subtitle_movie && $Program{$title}{$channel}{$time}{subtitle} =~ /$subtitle_movie/) {
    print "SubTitle-Movie hit\n" if ($debug & 64);
  }
  elsif ($Program{$title}{$channel}{$time}{description} && $Program{$title}{$channel}{$time}{description} =~ /$$rContext{descriptionRE}/i) {
    print "Description hit\n" if ($debug & 64);
  } else {
    # No "Fast"-hit. Exiting
    return "Nothing";
  }

  # Now look if we have a "exact" hit
  print "In Exact Hit Loop\n" if ($debug & 64);
  foreach my $counter (0 .. $$rContext{timercount}) {

    print "Before Title Match\n" if ($debug & 64);
    if ($$rContext{title}[$counter]) {	
      print "In Title Match \"$$rContext{title}[$counter]\"\n" if ($debug & 64);
      if (!($title =~ /$$rContext{title}[$counter]/i)) {
	print "Title rejected\n" if ($debug & 64);
	next;
      }
    }

    print "Before Subtitle Match\n" if ($debug & 64);
    if ($$rContext{subtitle}[$counter]) {
      print "In Subtitle Match \"$$rContext{subtitle}[$counter]\"\n" if ($debug & 64);
      if ($Program{$title}{$channel}{$time}{subtitle}) {
	if ($$rContext{subtitle}[$counter] =~ /^movie$/i) {
	  if (!($Program{$title}{$channel}{$time}{subtitle} =~ /$subtitle_movie/i)) {
	    print "Subtitle rejected 1\n" if ($debug & 64);
	    next;
	  }
	}
	elsif ($$rContext{subtitle}[$counter] =~ /^\!movie$/i) {
	  if (($Program{$title}{$channel}{$time}{subtitle} =~ /$subtitle_movie/i)) {
	    print "Subtitle rejected 2\n" if ($debug & 64);
	    next;
	  }
	}
	elsif (!($Program{$title}{$channel}{$time}{subtitle} =~ /$$rContext{subtitle}[$counter]/i)) {
	  print "Subtitle rejected 3\n" if ($debug & 64);
	  next;
	}
      } else {
	# We had a Subtitle, but epg.data did not have a subtitle for this record so no chance to record this
	print "Subtitle rejected 4\n" if ($debug & 64);
	next;
      }
    }

    print "Before Description Match\n" if ($debug & 64);
    if ($$rContext{description}[$counter]) {
      print "In Description Match \"$$rContext{description}[$counter]\"\n" if ($debug & 64);
      if ($Program{$title}{$channel}{$time}{description}) {
	if (!($Program{$title}{$channel}{$time}{description} =~ /$$rContext{description}[$counter]/i)) {
	  print "Description rejected 1\n" if ($debug & 64);
	  next;
	}
      }
      elsif (!$$rContext{title}[$counter] && !$$rContext{subtitle}[$counter]) {
	print "Description rejected 2\n" if ($debug & 64);
	next;
      }
    }

    print "Before Category Match\n" if ($debug & 64);
    if ($$rContext{category}[$counter]) {
      print "In Category Match \"$$rContext{category}[$counter]\"\n" if ($debug & 64);
      if ($Program{$title}{$channel}{$time}{category}) {
	my ($left, $right);
	($left, $right) = split (/\//, $$rContext{category}[$counter]);
	if ($left) {
	  print "In Category Match Left \"$left\"\n" if ($debug & 64);
	  if (!($Program{$title}{$channel}{$time}{category} =~ /^$left\//)) {
	    print "Category rejected 1\n" if ($debug & 64);
	    next;
	  }
	}
	if ($right) {
	  print "In Category Match Right \"$right\"\n" if ($debug & 64);
	  if (!($Program{$title}{$channel}{$time}{category} =~ /\/$right$/)) {
	    print "Category rejected 2\n" if ($debug & 64);
	    next;
	  }
	}
      } else {
	# We had a Category, but the epg.data not. So discard this Entry
	print "Category rejected 3\n" if ($debug & 64);
	next;
      }
    }

    print "Before Channel Match\n" if ($debug & 64);
    if ($$rContext{channel}[$counter]) {
      print "In Channel Match Whitelist-Mode \"$$rContext{channel}[$counter]\"\n" if ($debug & 64);
      if (!($channel =~ /$$rContext{channel}[$counter]/)) {
	print "Channel rejected\n" if ($debug & 64);
	next;
      }
    }

    if ($$rContext{blackchannel}[$counter]) {
      print "In Channel Match Blacklist-Mode \"$$rContext{blackchannel}[$counter]\"\n" if ($debug & 64);
      if ($channel =~ /$$rContext{blackchannel}[$counter]/) {
	print "Channel rejected\n" if ($debug & 64);
	next;
      }
    }

    print "Before Timeframe Match\n" if ($debug & 64);
    if ($$rContext{timeframe}[$counter]) {
      print "In Timeframe Match \"$$rContext{timeframe}[$counter]\"\n" if ($debug & 64);
      my (@time, $time2, $ctime, $ctime2);
      @time = GetTime($time);
      $time2 = "$time[0]$time[1]";
	
      ($ctime, $ctime2) = split (/\-/,$$rContext{timeframe}[$counter]);
	
      if (!$ctime) {
	$ctime = "0";
      }
      if (!$ctime2) {
	$ctime2 = "2400";
      }

      if ($ctime < $ctime2) {
	if (!($time2 >= $ctime && $time2 <= $ctime2)) {
	  print "Timeframe rejected 1\n" if ($debug & 64);
	  next;
	}
      }
      else {
	if (!(($time2 >= $ctime && $time2 <= "2400") || ($time2 >= "0" && $time2 <= $ctime2))) {
	  print "Timeframe rejected 2\n" if ($debug & 64);
	  next;
	}
      }
    }

    print "Before Weekday Match\n" if ($debug & 64);
    if ($$rContext{weekday}[$counter]) {
      print "In Weekday Match \"$$rContext{weekday}\"\n" if ($debug & 64);
      my ($wday);
      $wday = getWDay($time);
      $$rContext{weekday}[$counter] =~ /(.)(.)(.)(.)(.)(.)(.)/;
      if ($$wday eq "-") {
	print "Weekday rejected\n" if ($debug & 64);
	next;
      }
    }

    print "Before Minlength Match\n" if ($debug & 64);
    if ($$rContext{minlength}[$counter]) {
      print "In Minlength Match \"$$rContext{minlength}[$counter]\"\n" if ($debug & 64);
      if ($Program{$title}{$channel}{$time}{duration} < $$rContext{minlength}[$counter]) {
	print "Minlength rejected\n" if ($debug & 64);
	next;
      }
    }

    print "Before Maxlength Match\n" if ($debug & 64);
    if ($$rContext{maxlength}[$counter]) {
      print "In Maxlength Match \"$$rContext{maxlength}[$counter]\"\n" if ($debug & 64);
      if ($Program{$title}{$channel}{$time}{duration} > $$rContext{maxlength}[$counter]) {
	print "Maxlength rejected\n" if ($debug & 64);
	next;
      }
    }

    # All test passed. Accept this timer
    print "All Tests passed entry accepted/blacklisted\n" if ($debug & 64);
    return ($counter);
    }
  # Foreach ran out without a hit
  return "Nothing";
}

# Open the connection to VDR
sub initsocket {
  my ($Dest, $Port) = split (/\:/,$Dest[$currentVDR - 1],2);
  my $iaddr = inet_aton($Dest);
  my $paddr = sockaddr_in($Port, $iaddr);
  my $Timeout = 10; # max. seconds to wait for response

  $SIG{ALRM} = sub { die("Timeout while connecting to VDR"); };
  alarm($Timeout);

  socket(SOCKET, PF_INET, SOCK_STREAM, getprotobyname('tcp'));
  connect(SOCKET, $paddr) or die ("Can't connect to VDR\n");
  select(SOCKET); $| = 1;
  select(STDOUT);

  while (<SOCKET>) {
    last if substr($_, 3, 1) ne "-";
  }
  alarm(0);
}

# Send a command to VDR and read back the result
sub GetSend {
  my ($command, @retval);

    while ($command = shift) {
      print SOCKET "$command\r\n";
      while (<SOCKET>) {
	s/\x0d//g;
	(@retval) = (@retval, $_);
	last if substr($_, 3, 1) ne "-";
      }
    }
  return (@retval);
}

# Close the socket to VDR
sub closesocket {
  print SOCKET "Quit\r\n";
  close(SOCKET);
}


# Fetch the timers-List from VDR via SVDR and process it.
sub fetchVDRTimers {
  my (@timers, $timer, $position, $active, $channel, $day, $start, $end, $prio, $lifetime, $title, $subtitle, $minute, $duration);
  my ($utime, $utime2);

  # First fetch the timers-list from VDR
  @timers = GetSend ("lstt");

  foreach $timer (@timers) {
    chomp $timer;
    # a Valid Timer-line beginns with "250"
    if ($timer =~ s/250-|250\s//) {
      # Extract the Position in front of the line
      ($position, $timer) = split (/\s/,$timer,2);

#      print "Position: \"$position\" Timer: \"$timer\"\n";
      # Split the : seperated values
      ($active, $channel, $day, $start, $end, $prio, $lifetime, $title, $subtitle) = split (/\:/,$timer,9);

      my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);

      # If the string is exactly 7 char wide, then its a "repeating"-timer
      if ($active >= 1) {
	if ($day =~ /(.)(.)(.)(.)(.)(.)(.)/) {
	  my (@days);
	  @days = ($1, $2, $3, $4, $5, $6, $7);
	  ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);

	  $start =~ /(\d\d)(\d\d)/;
	  $hour = $1;
	  $minute = $2;
	  $utime = timelocal 0, $minute, $hour, $mday, $mon, $year;
	  $end =~ /(\d\d)(\d\d)/;
	  $hour = $1;
	  $minute = $2;
	  $utime2 = timelocal 0, $minute, $hour, $mday, $mon, $year;
	  if ($end < $start) {
	    $utime2 += 24*60*60;
	  }
	  $duration = $utime2 - $utime;

	  # "Normalize" the timestamp to monday
	  $utime = $utime - ($wday * 24 * 60 *60);

	  foreach my $num (0 .. $#days) {
	    if ($days[$num] ne "-") {
	      my $utime3;
	      # Days before today will be shifted in the next week
	      if (($num + 1) < $wday) {
		$utime3 = $utime + (($num + 7 + 1) * 24 * 60 * 60);
	      }
	      else {
		$utime3 = $utime + (($num + 1) * 24 * 60 * 60);
	      }
	      &addtimer ($title,$title,$subtitle,"",$channels[$channel],$utime3,$duration,$prio,$lifetime,"R$position",0,0);
	    }
	  }
	}

	# When the Day-Value is between 1 and 31, then its a "One time" Timer
	elsif (($day >= 1) && ($day <= 31)) {
	  if ($active == "2") {
	    $position += 1000000;
	  }
	  # When the Day is before the Current-Day, then the Timer is for the next month
	  if ($day < $mday) {
	    $mon++;
	    if ($mon == 12) {
	      $mon = 0;
	      $year ++;
	    }
	  }
	  $start =~ /(\d\d)(\d\d)/;
	  $hour = $1;
	  $minute = $2;
	  $utime = timelocal 0, $minute, $hour, $day, $mon, $year;
	  $end =~ /(\d\d)(\d\d)/;
	  $hour = $1;
	  $minute = $2;
	  $utime2 = timelocal 0, $minute, $hour, $day, $mon, $year;
	  if ($end < $start) {
	    $utime2 += 24*60*60;
	  }
	  $duration = $utime2 - $utime;

	  &addtimer ($title,$title,$subtitle,"",$channels[$channel],$utime,$duration,$prio,$lifetime,$position,0,0);
	}
      }
    }
  }
}

# Parse file "epg.data"
sub initepgdata {
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
	    undef $category;

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
	  elsif (/^K\s(.*)/) {
	    $category=$1;
	  }
	  # End Timer
	  elsif (/^e$/) {
	    # Only accept timers that are in the future
	    if ($time < time) {
	      next;
	    }
	    # Only accept timers that are at least 2 Seconds long
	    if ($duration <= 1) {
	      next;
	    }

	    # Work around the different Bugs in the data
	    &correct_epg_data();

	    # Check if the Title & Subtitle is in the Done-List (Only if Subtitle exists)
	    if ($subtitle && $title =~ /$title_done/ && $subtitle =~ /$subtitle_done/) {
	      next;
	    }

	    $Program{$title}{$channel}{$time}{duration}=$duration;
	    if ($subtitle) {
	      $Program{$title}{$channel}{$time}{subtitle}=$subtitle;
	    }
	    if ($description) {
	      $Program{$title}{$channel}{$time}{description}=$description;
	    }
	    if ($category) {
	      $Program{$title}{$channel}{$time}{category}=$category;
	    }
	    # Check if the title is in the DEEP-Blacklist
	    if (&testtimer("deepblack", $title, $channel, $time) ne "Nothing") {
	      print "Deepblack: \"$title\"" if ($debug & 64);
	      print " $subtitle" if ($debug & 64 && $subtitle);
	      print "\n" if ($debug & 64);
	      &delprogram ($title, $channel, $time);
	    }
	  }
	}
      }
    }
  close (FI);
}

# What is a Movie (When correctly stored into Subtitle)
sub initmovie {
  my (@list,$list);
  open (FI,"${configdir}/subtitle-movie") or return;
  @list = <FI>;
  close(FI);

  foreach $list (@list) {
    chomp $list;
  }
  $subtitle_movie = join ('|',@list);
}

# What is already recorded/Should not be recorded
sub initdone {
  my (@list,$list, %title_done, %subtitle_done, $title_temp, $subtitle_temp);
  open (FI,"${configdir}/done") or return;
  @list = <FI>;
  close (FI);

  foreach $list (@list) {
    chomp $list;
    ($title_temp,$subtitle_temp) = split (/\|/,$list);
    if ($title_temp) {
      $title_done{"^\Q$title_temp\E\$"} = 1;
    }
    if ($subtitle_temp) {
      $subtitle_done{"^\Q$subtitle_temp\E\$"} = 1;
    }
  }
  $title_done = join ('|',sort keys %title_done);
  $subtitle_done = join ('|',sort keys %subtitle_done);
}

sub processdone {
  # Now delete Timers in VDR that are already in the done-List
  my ($list, @list, $position, $timer, $active, $g, $title, $subtitle, $counter, @todel);
  $counter = 0;
  @list = GetSend ("LSTT");

  foreach $timer (@list) {
    chomp $timer;
    if ($timer =~ s/250-|250\s//) {
      ($position, $timer) = split (/\s/,$timer,2);
      # Split the : seperated values
      ($active, $g, $g, $g, $g, $g, $g, $title, $subtitle) = split (/\:/,$timer,9);
      if ($active == 2) {
        # Title: "Shakespeare in Love"||Subtitle: "Romanze"
        my ($ctitle, $csubtitle);
        if ($subtitle && $subtitle =~ /^Title\:\s\"(.*)\"\|\|Subtitle\:\s\"(.*)\"/) {
          $title = $1;
          $subtitle = $2;
          if ($subtitle) {
            my (@titles, @subtitles, $num, $hit);
            undef $hit;
            @titles = split (/\~/,$title);
            @subtitles = split (/\~/,$subtitle);
            foreach $num (0 .. $#titles) {
              if ($titles[$num] =~ /$title_done/ && $subtitles[$num] =~ /$subtitle_done/) {
                $hit = 1;
              }
              else {
                undef $hit;
                last;
              }
            }

            if ($hit) {
              my ($result);
              print "Delete Timer: $title $subtitle\n" if ($debug & 4);
              $position -= $counter;
              ($result) = GetSend ("DELT $position");
              print "Result: $result" if ($debug & 4);
              if ($result =~ /^250/) {
                $counter++;
              }
            }
          }
        }
      }
    }
  }
}

# What should be recorded
sub inittorecord {
  my ($context) = shift;
  my ($rContext);
  my (@title_list, @subtitle_list, @description_list, $line);
  my (%Input);
  my $counter = 0;

  if ($context eq "torecord") {
    $rContext = \%torecord;
    open (FI,"${configdir}/${context}") or die ("Can't open file \"$context\"\n");
  } elsif ($context eq "deepblack") {
    $rContext = \%deepblack;
    open (FI,"${configdir}/${context}") or return;
  } else {
    die ("Illegal Context");
  }


 outer: while (<FI>) {
    chomp if ($_);
    if ($_ && !(/^\#/) && /^\[.*\]$/) {
      $line = $.;
      undef %Input;
      while (<FI>) {
	chomp;
	if ($_ && !(/^\#/)) {
	  if (/^\[.*?\]$/) {
	    last;
	  }

	  my ($key, $value);
	  ($key, $value) = split (/\s+=\s+/);

	  if ($key =~ /^title$/i) {
	    if ($Input{title}) {
	      $Input{title} .= "|$value";
	    } else {
	      $Input{title} = $value;
	    }
	    print "Titel = $value\n" if ($debug & 16);
	  }
	  elsif ($key =~ /^subtitle$/i) {
	    if ($Input{subtitle}) {
	      $Input{subtitle} .= "|$value";
	    } else {
	      $Input{subtitle} = $value;
	    }
	    print "Subtitel = $value\n" if ($debug & 16);
	  }
	  elsif ($key =~ /^description$/i) {
	    if ($Input{description}) {
	      $Input{description} .= "|$value";
	    } else {
	      $Input{description} = $value;
	    }
	    print "Description = $value\n" if ($debug & 16);
	  }
	  elsif ($key =~ /^category$/i) {
	    $Input{category} = $value;
	    print "Category = $value\n" if ($debug & 16);
	  }
	  elsif ($key =~ /^channel$/i) {
	    if ($Input{channel}) {
	      $Input{channel} .= "|^$value\$";
	    } else {
	      $Input{channel} = $value;
	    }
	    print "Channel = $value\n" if ($debug & 16);
	  }
	  elsif ($key =~ /^timeframe$/i) {
	    $Input{timeframe} = $value;
	    print "Timeframe = $value\n" if ($debug & 16);
	  }
	  elsif ($key =~ /^weekday$/i) {
	    $Input{weekday} = $value;
	    print "Weekday = $value\n" if ($debug & 16);
	  }
	  elsif ($key =~ /^minlength$/i) {
	    $Input{minlength} = $value;
	    print "Minlength = $value\n" if ($debug & 16);
	  }
	  elsif ($key =~ /^maxlength$/i) {
	    $Input{maxlength} = $value;
	    print "Maxlength = $value\n" if ($debug & 16);
	  }
	  elsif ($key =~ /^prio$/i) {
	    $Input{prio} = $value;
	    print "Prio = $value\n" if ($debug & 16);
	  }
	  elsif ($key =~ /^lifetime$/i) {
	    $Input{lifetime} = $value;
	    print "Lifetime = $value\n" if ($debug & 16);
	  }
	  elsif ($key =~ /^timertitle$/i) {
	    $Input{timertitle} = $value;
	    print "Timertitel = $value\n" if ($debug & 16);
	  }
	  elsif ($key =~ /^margin$/i) {
	    $Input{margin} = $value;
	    print "Margin = $value\n" if ($debug & 16);
	  }
	  elsif ($key =~ /^instance$/i) {
	    $Input{instance} = $value;
	    print "Instance = $value\n" if ($debug & 16);
	  } else {
	    print "Unkown Key: \"$key\" with Value: \"$value\"\n";
	  }
	}
      }

      # Accept entry only if it is for the current instance or for "no" instance
      if (($Opts{s} && $Input{instance} && $Input{instance} eq "s") || !$Input{instance} || ($Input{instance} ne "s" && $Input{instance} == $currentVDR)) {
	# Accept entry only if at least a Title/Subtitle/Description is provied
	if (!$Input{title} && !$Input{subtitle} && !$Input{description}) {
	  print "No Title/Subtitle/Description Field. $context entry ignored. Block beginning at Line $line\n";
	  redo outer;
	}

	if ($Input{title}) {
	  $$rContext{title}[$counter] = $Input{title};
	  $title_list[$#title_list + 1] = $Input{title};
	}
	if ($Input{subtitle}) {
	  if ($Input{subtitle} =~ /^movie$/i || $Input{subtitle} =~ /^\!movie$/i) {
	    $test_subtitle_movie = 1;
	  }
	  $$rContext{subtitle}[$counter] = $Input{subtitle};
	  $subtitle_list[$#subtitle_list + 1] = $Input{subtitle};
	}
	if ($Input{description}) {
	  $$rContext{description}[$counter] = $Input{description};
	  $description_list[$#description_list + 1] = $Input{description};
	}
	if ($Input{category}) {
	  $$rContext{category}[$counter] = $Input{category};
	}
	if ($Input{channel}) {
	  if ($Input{channel} =~ /\!/) {
	    $Input{channel} =~ s/\!//g;
	    $$rContext{blackchannel}[$counter] = $Input{channel};
	  } else {
	    $$rContext{channel}[$counter] = $Input{channel};
	  }
	}
	if ($Input{timeframe}) {
	  $$rContext{timeframe}[$counter] = $Input{timeframe};
	}
	if ($Input{weekday}) {
	  $$rContext{weekday}[$counter] = $Input{weekday};
	}
	if ($Input{minlength}) {
	  if ($Input{minlength} =~ /^(\d+)m$/) {
	    $Input{minlength} = $1 * 60
	  } elsif ($Input{minlength} =~ /^(\d+)h$/) {
	    $Input{minlength} = $1 * 60 * 60
	  }
	  $$rContext{minlength}[$counter] = $Input{minlength};
	}
	if ($Input{maxlength}) {
	  if ($Input{maxlength} =~ /^(\d+)m$/) {
	    $Input{maxlength} = $1 * 60
	  } elsif ($Input{maxlength} =~ /^(\d+)h$/) {
	    $Input{maxlength} = $1 * 60 * 60
	  }
	  $$rContext{maxlength}[$counter] = $Input{maxlength};
	}
	if ($Input{prio}) {
	  $$rContext{prio}[$counter] = $Input{prio};
	}
	if ($Input{lifetime}) {
	  $$rContext{lifetime}[$counter] = $Input{lifetime};
	}
	else {
	  $$rContext{prio}[$counter] = $default_prio;
	}
	if ($Input{timertitle}) {
	  $$rContext{timertitle}[$counter] = $Input{timertitle};
	}
	if ($Input{margin}) {
	  my ($start, $stop);
	  ($start, $stop) = split (/;/,$Input{margin}, 2);
	  $$rContext{marginstart}[$counter] = $start if ($start);
	  $$rContext{marginstop}[$counter] = $stop if ($stop);
	}
	# Set Default-Margins if no margins defined
	$$rContext{marginstart}[$counter] = $marginstart if (!$$rContext{marginstart}[$counter]);
	$$rContext{marginstop}[$counter] = $marginstop if (!$$rContext{marginstop}[$counter]);
	$counter++;
	if ($Input{instance}) {
	  $$rContext{instance}[$counter] = $Input{instance};
	}
      }
      redo outer;
    }
  }

  $$rContext{timercount} = $counter - 1;

  $$rContext{titleRE} = join ('|',@title_list);
  if ($$rContext{titleRE} && $$rContext{titleRE} =~ /\|.\|/) {
    $$rContext{titleRE} = ".";
  }
  $$rContext{subtitleRE} = join ('|',@subtitle_list);
  if ($$rContext{subtitleRE} && $$rContext{subtitleRE} =~ /\|.\|/) {
    $$rContext{subtitleRE} = ".";
  }
  $$rContext{descriptionRE} = join ('|',@description_list);
  if ($$rContext{descriptionRE} && $$rContext{descriptionRE} =~ /\|.\|/) {
    $$rContext{descriptionRE} = ".";
  }

  if (!$$rContext{titleRE}) {
    $$rContext{titleRE} = "^Dieseshierwirdgarantiertnieundnimmeraufirgendetwassinnvollesmatchen\$";
  }
  if (!$$rContext{subtitleRE}) {
    $$rContext{subtitleRE} = "^Dieseshierwirdgarantiertnieundnimmeraufirgendetwassinnvollesmatchen\$";
  }
  if (!$$rContext{descriptionRE}) {
    $$rContext{descriptionRE} = "^Dieseshierwirdgarantiertnieundnimmeraufirgendetwassinnvollesmatchen\$";
  }
}

# Parse "LSTC"-Command of VDR
sub initchannellist {
  my ($counter, $chan, $garbage, $card, @temp_channels, $temp, $i);

  @temp_channels = GetSend ("LSTC");

  foreach $i (0 .. $#temp_channels) {
    $temp = $temp_channels[$i];
    chomp $temp;

    if ($temp =~ s/250-|250\s//) {
      ($counter, $temp) = split (/\s/,$temp,2);
      ($chan, $garbage,$garbage, $garbage, $garbage, $garbage, $garbage, $card, $garbage) = split (/\:/,$temp);
      $channels[$counter] = $chan;
      $channels{$chan}{number} = $counter;
      $channels{$chan}{card} = $card;
      $counter++;
    }
  }
}

sub initconfigfile {
  open (FI,"${configdir}/config") or return;
  while (<FI>) {
    s/\#.*//;
    chomp;
    if ($_) {
      my ($key, $value);
      ($key, $value) = split (/\s+=\s+/);
      if ($key =~ /^debug$/i) {
	$debug = $value;
	print "Debug-Level = $value\n" if ($debug & 16);
      }
      elsif ($key =~ /^marginstart$/i) {
	print "Marginstart = $value\n" if ($debug & 16);
	$marginstart = $value;
      }
      elsif ($key =~ /^marginstop$/i) {
	print "Marginstop = $value\n" if ($debug & 16);
	$marginstop = $value;
      }
      elsif ($key =~ /^DVBCards$/i) {
	print "DVB_Cards = $value\n" if ($debug & 16);
	$DVB_cards = $value;
      }
      elsif ($key =~ /^defaultprio$/i) {
	print "Default Priority = $value\n" if ($debug & 16);
	$default_prio = $value;
      }
      elsif ($key =~ /^Dest$/i) {
	print "Destination Host/IP:Port = $value\n" if ($debug & 16);
	@Dest = split (/\s+/,$value);
      }
      elsif ($key =~ /^jointimers$/i) {
	print "Join Timers = $value\n" if ($debug & 16);
	$jointimers = $value;
      }
      elsif ($key =~ /^description$/i) {
	print "Description = $value\n" if ($debug & 16);
	$Description = $value;
      }
      else {
	print "Unkown Key: \"$key\" with Value: \"$value\"\n";
      }
    }
  }
  print "End Config\n" if ($debug & 16);
}

sub initcommandline() {
  my $Usage = qq{
Usage: $0 [options] [Instance]...

Options: -d hostname:Port   hostname/ip:Port (localhost:2001)
         -c configdir       Directory where all config files are located
                            (~/.master-timer)
         -i instance        Which VDR-Instance, from the config-file, should be
                            used
         -s                 Print all series from epg.data and exit
         -v debuglevel      Level of debug-messages to print
         -h                 This Help-Page
};

  # Only process commandline if not already processed
  if (!$Opts{done}) {
    die $Usage if (!getopts("d:p:c:i:sv:h",\%Opts));
  }
  die $Usage if ($Opts{h});
  # Mark the options as already processed
  $Opts{done} = 1;

  if ($Opts{v}) {
    $debug = $Opts{v};
  }
  if ($Opts{i}) {
    $currentVDR = $Opts{i};
  }
  if ($Opts{d}) {
    @Dest = ($Opts{d});
  }
  if ($Opts{c}) {
    $configdir = $Opts{c};
  }
}

sub init {
  &initcommandline();
  &initconfigfile();
  # Process commandline a second time, so that configs from the config-file are overwritten
  &initcommandline();
  &initsocket();
  &initmovie();
  &initdone();
  &initchannellist();
  &inittorecord("deepblack");
  &initepgdata();
  &inittorecord("torecord");
}
