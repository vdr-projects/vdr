#!/usr/bin/perl -w

use strict;
# For the TCP-Connection to VDR
use Socket;
# For converting the Timers, read from VDR, back to Unix-Timestamps
use Time::Local;

# Debugmode
# You have to add the following numbers to build the debug-var
# 1     : Dump the "torecord"
# 2     : Dump all timers
# 4     : Show when a timer will be deleted
# 8     : Dump the "Done" REs
# 16    : Verbose Config-Reading
my $debug = 0;

# The Supervariable Program
# %Program{$title}{$channel}{$time}{duration}
#                                  {subtitle}
#                                  {description}

# The Supervariable Timer
# %Timer{$time}{$channel}{$title}{duration}
#                                {subtitle}
#                                {prio}
#                                {real_title}
#                                {VDR} (Already programmed)
# The Value of VDR is ">0" for the position in the Timer-List or "R" for a "Repeating" Timer.
# A Value of >1.000.000 is a Master Timer-Timer which is already programmed into VDR

# Variable-Definition
my (%Program, @channels, %channels, %Timer);

# Which Subtitles are Movies
my ($subtitle_movie);

# Blacklist
my ($title_deepblack);

# What is already recorded/Should not be recorded
my ($title_done, $subtitle_done);

# What to record
my ($title_torecord, $subtitle_torecord, $description_torecord, @title_torecord, @subtitle_torecord, @description_torecord, @channel_torecord, @timeframe_torecord, @prio_torecord, @timer_title_torecord, $num_torecord, @marginstart_torecord, @marginstop_torecord, @machine_torecord);

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

# Working-Variables
my ($title, $duration, $subtitle, $channel, $time, $description, $hit);
my (@time, @date);

sub sub_die
  {
    my ($error) = @_;
    &closesocket();
    die "$error";
  }


if ($ARGV[0])
  {
    $currentVDR = $ARGV[0];
  }

&init();
&dumpdone() if ($debug & 8);
&dumptorecord() if ($debug & 1);
&fetchVDRTimers();
&process_torecord();
print "Timers before joining\n" if ($debug & 2 && $jointimers);
&dumptimers() if ($debug & 2);
if ($jointimers)
  {
    &jointimers();
    print "Timers after joining\n" if ($debug & 2);
    &dumptimers() if ($debug & 2);
  }

&printtimers();
&transfertimers();
&closesocket();

#
# Subfunctions
#

sub dumpdone()
  {
    print "Start Done-dump\n";
    print "Titledone: \"$title_done\"\n";
    print "Subtitledone \"$subtitle_done\"\n";
    print "End Done-dump\n";
  }

sub dumptorecord()
  {
    print "Start Torecord-dump\n";
    print "Regex-Title: $title_torecord\n";
    print "Regex-Subtitle: $subtitle_torecord\n";
    print "Regex-Description: $description_torecord\n";
    foreach my $num (0 .. $num_torecord)
      {
	print "Timer Number $num: ";

	print "Title: \"$title_torecord[$num]\" " if ($title_torecord[$num]);
	print "Title: \"\" " unless ($title_torecord[$num]);

	print "Subtitle: \"$subtitle_torecord[$num]\" "if ($subtitle_torecord[$num]);
	print "Subtitle: \"\" " unless ($subtitle_torecord[$num]);
	
	print "Description: \"$description_torecord[$num]\" " if ($description_torecord[$num]);
	print "Description: \"\" " unless ($description_torecord[$num]);
	
	print "Timeframe: \"$timeframe_torecord[$num]\" " if ($timeframe_torecord[$num]);
	print "Timeframe: \"\" " unless ($timeframe_torecord[$num]);
	
	print "Channel: \"". join (";",@{$channel_torecord[$num]})."\" " if ($channel_torecord[$num]);
	print "Channel: \"\" " unless ($channel_torecord[$num]);
	
	print "Prio: \"$prio_torecord[$num]\" " if ($prio_torecord[$num]);
	print "Prio: \"\" " unless ($prio_torecord[$num]);
	
	print "Timertitle: \"$timer_title_torecord[$num]\" " if ($timer_title_torecord[$num]);
	print "Timertitle: \"\" " unless ($timer_title_torecord[$num]);

	print "Marginstart: \"$marginstart_torecord[$num]\" " if ($marginstart_torecord[$num]);
	print "Marginstart: \"\" " unless ($marginstart_torecord[$num]);

	print "Marginstop: \"$marginstop_torecord[$num]\" " if ($marginstop_torecord[$num]);
	print "Marginstop: \"\" " unless ($marginstop_torecord[$num]);

	print "Machine: \"$machine_torecord[$num]\" " if ($machine_torecord[$num]);
	print "Machine: \"\" " unless ($machine_torecord[$num]);

	print "\n";
      }
    print "End Torecord-dump\n";
  }

sub dumptimers()
  {
    print "Start Timers-dump\n";
    foreach $time (sort {$a <=> $b} keys %Timer)
      {
	foreach $channel (sort keys %{%Timer->{$time}})
	  {
	    foreach $title (sort keys %{%Timer->{$time}->{$channel}})
	      {
		my ($prio, @time, @date, @time2);
		my ($realtitle);
		@time = &GetTime ($time);
		@date = &GetDay ($time);
		@time2 = &GetTime ($time + $Timer{$time}{$channel}{$title}{duration});
		$subtitle = $Timer{$time}{$channel}{$title}{subtitle};
		$prio = $Timer{$time}{$channel}{$title}{prio};
		$realtitle = $Timer{$time}{$channel}{$title}{real_title};
		print "2:$channels{$channel}{number}:$date[1]:$time[0]$time[1]:$time2[0]$time2[1]:$prio:99:$title:Title: \"$realtitle\"||Subtitle: \"$subtitle\":$Timer{$time}{$channel}{$title}{VDR}\n";
	      }
	  }
      }
    print "End Timers-dump\n";
  }

sub printtimers()
  {
    foreach $time (sort {$a <=> $b} keys %Timer)
      {
	foreach $channel (sort keys %{%Timer->{$time}})
	  {
	    foreach $title (sort keys %{%Timer->{$time}->{$channel}})
	      {
		my ($prio, @time, @date, @time2);
		if ($Timer{$time}{$channel}{$title}{VDR} eq 0)
		  {
		    my ($realtitle);
		    @time = &GetTime ($time);
		    @date = &GetDay ($time);
		    @time2 = &GetTime ($time + $Timer{$time}{$channel}{$title}{duration});
		    $subtitle = $Timer{$time}{$channel}{$title}{subtitle};
		    $prio = $Timer{$time}{$channel}{$title}{prio};
		    $realtitle = $Timer{$time}{$channel}{$title}{real_title};

		    print "2:$channels{$channel}{number}:$date[1]:$time[0]$time[1]:$time2[0]$time2[1]:$prio:99:$title:Title: \"$realtitle\"||Subtitle: \"$subtitle\"\n";
		  }
	      }
	  }
      }
  }

sub transfertimers()
  {
    foreach $time (sort {$a <=> $b} keys %Timer)
      {
	foreach $channel (sort keys %{%Timer->{$time}})
	  {
	    foreach $title (sort keys %{%Timer->{$time}->{$channel}})
	      {
		my ($prio, @time, @date, @time2, $realtitle, $result);
		if ($Timer{$time}{$channel}{$title}{VDR} eq 0)
		  {
		    @time = &GetTime ($time);
		    @date = &GetDay ($time);
		    @time2 = &GetTime ($time + $Timer{$time}{$channel}{$title}{duration});
		    $subtitle = $Timer{$time}{$channel}{$title}{subtitle};
		    $prio = $Timer{$time}{$channel}{$title}{prio};
		    $realtitle = $Timer{$time}{$channel}{$title}{real_title};

		    ($result) = GetSend ("newt 2:$channels{$channel}{number}:$date[1]:$time[0]$time[1]:$time2[0]$time2[1]:$prio:99:$title:Title: \"$realtitle\"||Subtitle: \"$subtitle\"");
		    print "Timer: $result" if ($debug & 2);
		  }
	      }
	  }
      }
  }

# Convert the Unix-Time-Stamp into "month" and "Day of month"
sub GetDay
 {
    my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(shift);
    $mon++;
    $mon = sprintf ("%02i",$mon);
    $mday = sprintf ("%02i",$mday);
    return ($mon, $mday);
  }

# Convert the Unix-Time-Stramp into "hour" and "minute"
sub GetTime
  {
    my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(shift);
    $hour = sprintf ("%02i",$hour);
    $min = sprintf ("%02i",$min);
    return ($hour, $min);
  }

# Workaround some EPG-Bugs
sub correct_epg_data
  {
    if ($subtitle)
      {
	# For Pro-7. Remove $title from $subtitle
	$subtitle =~ s/$title\s\/\s//;
	
	# For VOX & VIVA. The Format it '"<Subtitle>". <Description>'
	if ($subtitle =~ /^\"(.*?)\"\.\s(.*)/)
	  {
	    # Lets see if there are Channels that where the VOX/VIVA scheme matches, but also have a description
	    if ($description)
	      {
		sub_die ("Subtitle: \"$subtitle\"\nDescription\"$description\"\n");
	      }
	    $subtitle = $1;
	    $description = $2;
	  }
	elsif ($channel eq "VIVA")
	  {
	    if ($subtitle =~ /^\s(.*)/)
	      {
		$subtitle = "";
		$description = $1;
	      }
	  }
      }

    # Workaround for the broken PRO-7/Kabel-1 EPG-Date. If Time is between 00.00 and 05.00 the time is shifted forward by a day
    if ($channel eq "Pro-7" || $channel eq "Kabel-1")
      {
	my (@time);
	@time = GetTime ($time);
	if ($time[0] >= 0 && ($time[0] <= 4 || ($time[0] == 5 && $time[1] == 0)))
	  {
	    $time += 24*60*60;
	  }
      }
}

# Add a Recording into the "to record"-List
sub addtimer
  {
    my ($hit, $title, $realtitle, $subtitle, $channel, $time, $duration, $prio, $VDR, $time2, $title2, $channel2, $marginstart, $marginstop);
    ($title, $realtitle, $subtitle, $channel, $time, $duration, $prio, $VDR, $marginstart, $marginstop) = @_;
#    print "Title: \"$title\" Realtitle: \"$realtitle\" Subtitle: \"$subtitle\" Channel: \"$channel\" Time: \"$time\" Duration: \"$duration\" Prio: \"$prio\" VDR: \"$VDR\"\n";

    $hit = 1;

    foreach $time2 (sort keys %Timer)
      {
        foreach $title2 (sort keys %{%Timer->{$time2}->{$channel}})
          {
	    my ($ctime, $ctime2);
	    $ctime = $time2;
	    $ctime2 = $time2 + $Timer{$time2}{$channel}{$title2}{duration};

	    if (($time >= $ctime) && ($time <= $ctime2))
	      {
		undef $hit;
	      }
          }
      }


    if ($hit)
      {
	$time -= $marginstart;
	$duration += $marginstart + $marginstop;
	$Timer{$time}{$channel}{$title}{duration}=$duration;
	$Timer{$time}{$channel}{$title}{subtitle}=$subtitle;
	$Timer{$time}{$channel}{$title}{prio}=$prio;
	$Timer{$time}{$channel}{$title}{VDR}=$VDR;
	$Timer{$time}{$channel}{$title}{real_title}=$realtitle;
      }
  }

sub deltimer()
  {
    my ($time, $channel, $title, $delete_from_VDR);
    ($time, $channel, $title, $delete_from_VDR) = @_;

#    if ($delete_from_VDR)
#      {
#	if ($Timer{$time}{$channel}{$title}{VDR})
#	  {
#	    if ($Timer{$time}{$channel}{$title}{VDR} =~ s/ ^R/)
#	      {
#		print "Error: A Repeating-Timer can't be deleted from VDR: \"$title\"\n";
#	      }
#	    elsif ($Timer{$time}{$channel}{$title}{VDR} < 1000000)
#	      {
#		print "A User-Programmed Timer has been deleted from VDR: \"$title\"\n";
#	      }
#	    else
#	      {
#		
#	      }
#	  }
#      }

    delete $Timer{$time}{$channel}{$title}{duration};
    delete $Timer{$time}{$channel}{$title}{subtitle};
    delete $Timer{$time}{$channel}{$title}{prio};
    delete $Timer{$time}{$channel}{$title}{VDR};
    delete $Timer{$time}{$channel}{$title}{real_title};
    delete $Timer{$time}{$channel}{$title};
    delete $Timer{$time}{$channel} if (keys %{ $Timer{$time}{$channel} } == 1);
    delete $Timer{$time} if (keys %{ $Timer{$time} } == 1);
  }

sub jointimers
  {
    #
    # FIXME: 2 Timers on the same channel will always be joined.
    # It should be checked if there is another DVB-Card available.
    #
    # FIXME2: When one timer is already programmed in VDR, delete that timer in VDR.
    my ($running, $counter, @times, $channel, $title, $channel2, $title2);
    $running = 1;
  outer: while ($running)
      {
	$counter = 0;
	@times = sort {$a <=> $b} keys %Timer;

	# We only need to check till the second last timer. The last one can't have a overlapping one.
	while ($counter < $#times)
	  {
	    foreach $channel (sort keys %{%Timer->{$times[$counter]}})
	      {
		foreach $title (sort keys %{%Timer->{$times[$counter]}->{$channel}})
		  {
		    if ($times[$counter + 1] < ($times[$counter] + $Timer{$times[$counter]}{$channel}{$title}{duration}))
		      {
			foreach $channel2 (sort keys %{%Timer->{$times[$counter + 1]}})
			  {
			    foreach $title2 (sort keys %{%Timer->{$times[$counter + 1]}->{$channel}})
			      {
				if ($channel eq $channel2)
				  {
				    my ($duration, $subtitle, $prio, $realtitle, $duration2, $subtitle2, $prio2, $realtitle2);
				    # Values from Lower-Timer
				    $duration = $Timer{$times[$counter]}{$channel}{$title}{duration};
				    $subtitle = $Timer{$times[$counter]}{$channel}{$title}{subtitle};
				    $prio = $Timer{$times[$counter]}{$channel}{$title}{prio};
				    $realtitle = $Timer{$times[$counter]}{$channel}{$title}{real_title};

				    # Values from Higher-Timer
				    $duration2 = $Timer{$times[$counter + 1]}{$channel2}{$title2}{duration};
				    $subtitle2 = $Timer{$times[$counter + 1]}{$channel2}{$title2}{subtitle};
				    $prio2 = $Timer{$times[$counter + 1]}{$channel2}{$title2}{prio};
				    $realtitle2 = $Timer{$times[$counter + 1]}{$channel2}{$title2}{real_title};

				    # Use the Higher Priority for the new Timer
				    $prio = ($prio > $prio2) ? $prio : $prio2;

				    # Delete the two "Obsolet" Timers
				    &deltimer ($times[$counter], $channel, $title);
				    &deltimer ($times[$counter + 1], $channel2, $title2);

				    # And set the new one
				    &addtimer ("$title + $title2", "$realtitle\~$realtitle2", "$subtitle\~$subtitle2", $channel, $times[$counter], $duration2 + ($times[$counter + 1 ] - $times[$counter]),$prio,0,0,0);

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

sub process_torecord
  {
    my ($first_hit, $prio, $timer_title);
    foreach $title (sort keys %Program)
      {
	foreach $channel (sort keys %{%Program->{$title}})
	  {
	    foreach $time (sort {$a <=> $b} keys %{%Program->{$title}->{$channel}})
	      {
		undef $hit;

		# First look if any of the Title/Subtitle/Description REs match
		if ($title =~ /$title_torecord/i)
		  {
		    $hit = 1;
		  }
		elsif ($Program{$title}{$channel}{$time}{subtitle} && $Program{$title}{$channel}{$time}{subtitle} =~ /$subtitle_torecord/i)
		  {
		    $hit = 1;
		  }
		elsif ($Program{$title}{$channel}{$time}{description} && $Program{$title}{$channel}{$time}{description} =~ /$description_torecord/i)
		  {
		    $hit = 1;
		  }

		# Now look if we have a "exact" hit
		if ($hit)
		  {
		    my ($counter);
		    undef $hit;
		    foreach $counter (0 .. $num_torecord)
		      {
			
			if ($title_torecord[$counter])
			  {	
			    if (!($title =~ /$title_torecord[$counter]/i))
			      {
				next;
			      }
			  }

			if ($subtitle_torecord[$counter])
			  {
			    if (!($Program{$title}{$channel}{$time}{subtitle} =~ /$subtitle_torecord[$counter]/i))
			      {
				next;
			      }
			    elsif (!$title_torecord[$counter] && !$description_torecord[$counter])
			      {
				next;
			      }
			  }

			if ($description_torecord[$counter])
			  {
			    if ($Program{$title}{$channel}{$time}{description})
			      {
				if (!($Program{$title}{$channel}{$time}{description} =~ /$description_torecord[$counter]/i))
				  {
				    next;
				  }
			      }
			    elsif (!$title_torecord[$counter] && !$subtitle_torecord[$counter])
			      {
				next;
			      }
			  }

			if ($channel_torecord[$counter])
			  {
			    my ($hit);
			    # Blacklist-Mode
			    if ($channel_torecord[$counter][0] =~ /^!/)
			      {
				$hit = 1;
				foreach (0 .. $#{$channel_torecord[$counter]})
				  {
				    # Strip a possibel "!" Charactar
				    $channel_torecord[$counter][$_] =~ /^!?(.*)/;
				    if ($channel =~ /^$1$/)
				      {
					undef $hit;
					last;
				      }
				  }
			      }
			    # Whitelist-Mode
			    else
			      {
				undef $hit;
				foreach (0 .. $#{$channel_torecord[$counter]})
				  {
				    # Strip a possibel "!" Charactar
				    $channel_torecord[$counter][$_] =~ /^!?(.*)/;
				    if ($channel =~ /^$1$/)
				      {
					$hit = 1;
					last ;
				      }
				  }
			      }
			    if (!$hit)
			      {
				next;
			      }
			  }

			if ($timeframe_torecord[$counter])
			  {
			    my (@time, $time2, $ctime, $ctime2);
			    @time = GetTime($time);
			    $time2 = "$time[0]$time[1]";

			    ($ctime, $ctime2) = split (/\-/,$timeframe_torecord[$counter]);

			    if (!$ctime)
			      {
				$ctime = "0";
			      }
			    if (!$ctime2)
			      {
				$ctime2 = "2400";
			      }

			    if ($ctime < $ctime2)
			      {
				if (!($time2 >= $ctime && $time2 <= $ctime2))
				  {
				    next;
				  }
			      }
			    else
			      {
				if (!(($time2 >= $ctime && $time2 <= "2400") || ($time2 >= "0" && $time2 <= $ctime2)))
				  {
				    next;
				  }
			      }
			  }

			if ($prio_torecord[$counter])
			  {
			    $prio = $prio_torecord[$counter];
			  }
			else
			  {
			    $prio = 50;
			  }

			# What Title to use for the timer
			if ($timer_title_torecord[$counter])
			  {
			    $timer_title = $timer_title_torecord[$counter]
			  }
			elsif ($title_torecord[$counter])
			  {
			    $timer_title = $title_torecord[$counter]
			  }
			else
			  {
			    $timer_title = $title;
			  }

			my ($subtitle);
			if ($Program{$title}{$channel}{$time}{subtitle})
			  {
			    $subtitle = $Program{$title}{$channel}{$time}{subtitle};
			  }
			else
			  {
			    $subtitle = "";
			  }

			&addtimer ($timer_title,$title,$subtitle,$channel,$time,$Program{$title}{$channel}{$time}{duration},$prio,0,$marginstart_torecord[$counter],$marginstop_torecord[$counter]);
			last;
		      }
		  }
	      }
	  }
      }
  }

# Open the connection to VDR
sub initsocket
  {
    my ($Dest, $Port) = split (/\:/,$Dest[$currentVDR - 1],2);
    my $iaddr = inet_aton($Dest);
    my $paddr = sockaddr_in($Port, $iaddr);

    socket(SOCKET, PF_INET, SOCK_STREAM, getprotobyname('tcp'));
    connect(SOCKET, $paddr) or sub_die ("Can't connect to VDR\n");
    select(SOCKET); $| = 1;
    select(STDOUT);

    while (<SOCKET>) {
      last if substr($_, 3, 1) ne "-";
    }
  }

# Send a command to VDR and read back the result
sub GetSend
  {
    my ($command, @retval);

    while ($command = shift)
      {
	print SOCKET "$command\r\n";
	while (<SOCKET>) {
	  (@retval) = (@retval, $_);
	  last if substr($_, 3, 1) ne "-";
	}
      }

    foreach my $retval (@retval)
      {
	$retval =~ s/\x0d//g;
      }
    return (@retval);
  }

# Close the socket to VDR
sub closesocket
  {
    print SOCKET "Quit\r\n";
    close(SOCKET);
  }


# Fetch the timers-List from VDR via SVDR and process it.
sub fetchVDRTimers
  {
    my (@timers, $timer, $position, $active, $channel, $day, $start, $end, $prio, $ttl, $title, $subtitle, $minute, $duration);
    my ($utime, $utime2);

    # First fetch the timers-list from VDR
    @timers = GetSend ("lstt");

    foreach $timer (@timers)
      {
#	$timer =~ s/\x0d//g;
	chomp $timer;
	# a Valid Timer-line beginns with "250"
	if ($timer =~ s/250-|250\s//)
	  {
	    # Extract the Position in front of the line
	    ($position, $timer) = split (/\s/,$timer,2);

#	    print "Position: \"$position\" Timer: \"$timer\"\n";
	    # Split the : seperated values
	    ($active, $channel, $day, $start, $end, $prio, $ttl, $title, $subtitle) = split (/\:/,$timer,9);

	    my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);

	    # If the string is exactly 7 char wide, then its a "repeating"-timer
	    if ($active >= 1)
	      {
		if ($day =~ /(.)(.)(.)(.)(.)(.)(.)/)
		  {
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
		    if ($end < $start)
		      {
			$utime2 += 24*60*60;
		      }
		    $duration = $utime2 - $utime;

		    # "Normalize" the timestamp to monday
		    $utime = $utime - ($wday * 24 * 60 *60);

		    foreach my $num (0 .. $#days)
		      {
			if ($days[$num] ne "-")
			  {
			    my $utime3;
			    # Todays before today will be shifted in the next week
			    if (($num + 1) < $wday)
			      {
				$utime3 = $utime + (($num + 7 + 1) * 24 * 60 * 60);
			      }
			    else
			      {
				$utime3 = $utime + (($num + 1) * 24 * 60 * 60);
			      }
			    &addtimer ($title,$title,$subtitle,$channels[$channel],$utime3,$duration,$prio,"R$position",0,0);
			  }
		      }
		  }

		# When the Day-Value is between 1 and 31, then its a "One time" Timer
		elsif (($day >= 1) && ($day <= 31))
		  {
		    if ($active == "2")
		      {
			$position += 1000000;
		      }
		    # When the Day is before the Current-Day, then the Timer is for the next month
		    if ($day < $mday)
		      {
			$mon++;
			if ($mon == 12)
			  {
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
		    if ($end < $start)
		      {
			$utime2 += 24*60*60;
		      }
		    $duration = $utime2 - $utime;

		    &addtimer ($title,$title,$subtitle,$channels[$channel],$utime,$duration,$prio,$position,0,0);
		  }
	      }
	  }
      }
  }

# Parse file "epg.data"
sub initepgdata
  {
    open (FI,"epg.data") or sub_die ("Can't open file \"epg.data\"\n");

    while (<FI>)
      {
	# Begin Channel
	if (/^C\s(\d+)\s+(.+)/)
	  {
	    $channel = $2;
	    while (<FI>)
	      {
		# End Channel
		if (/^c$/)
		  {
		    last;
		  }
		# Begin Timer
		elsif (/^E\s(\d+)\s+(\d+)\s+(\d+)$/)
		  {
		    # Undef this Variables because it is possibel that not every timer uses this values
		    undef $duration;
		    undef $subtitle;
		    undef $description;

		    $time=$2;
		    $duration=$3;
		  }
		# Title
		elsif (/^T\s(.*)/)
		  {
		    $title = $1;
		  }
		# Subtitle
		elsif (/^S\s(.*)/)
		  {
		    $subtitle=$1;
		  }
		# Description
		elsif (/^D\s(.*)/)
		  {
		    $description=$1;
		  }
		# End Timer
		elsif (/^e$/)
		  {
		    # Only accept timers that are in the future
		    if ($time < time)
		      {
			next;
		      }

		    # Work around the diffrent Bugs in the data
		    &correct_epg_data();

		    # Check if the title is in the DEEP-Blacklist
		    if ($title =~ /$title_deepblack/i)
		      {
			next;
		      }

		    # Check if the Title & Subtitle is in the Done-List
		    if ($title =~ /$title_done/)
		      {
			if ($subtitle)
			  {
			    if ($subtitle =~ /$subtitle_done/)
			      {
				next;
			      }
			  }
		      }

		    $Program{$title}{$channel}{$time}{duration}=$duration;
		    if ($subtitle)
		      {
			$Program{$title}{$channel}{$time}{subtitle}=$subtitle;
		      }
		    if ($description)
		      {
			$Program{$title}{$channel}{$time}{description}=$description;
		      }
		  }
	      }
	  }
      }
    close (FI);
  }

# What is a Movie (When correctly stored into Subtitle)
sub initmovie
  {
    my (@list,$list);
    open (FI,"$ENV{HOME}/.master-timer/subtitle-movie") or return;
    @list = <FI>;
    close(FI);

    foreach $list (@list)
      {
	chomp $list;
      }
    $subtitle_movie = join ('|',@list);
  }

# What should be blacklistet
sub initblacklist
  {
    my (@list,$list);
    if (open (FI,"$ENV{HOME}/.master-timer/deepblack"))
      {
	@list = <FI>;
	close(FI);
	
	foreach $list (@list)
	  {
	    chomp $list;
	  }
	$title_deepblack = join ('|',@list);
      }
    else
      {
	$title_deepblack = "^\$";
      }
  }

# What is already recorded/Should not be recorded
sub initdone
  {
    my (@list,$list, %title_done, %subtitle_done, $title_temp, $subtitle_temp);
    if (open (FI,"$ENV{HOME}/.master-timer/done"))
      {
	@list = <FI>;
	close (FI);

	foreach $list (@list)
	  {
	    chomp $list;
	    ($title_temp,$subtitle_temp) = split (/\|/,$list);
	    if ($title_temp)
	      {
		$title_done{"^$title_temp\$"} = 1;
	      }
	    if ($subtitle_temp)
	      {
		$subtitle_done{"^$subtitle_temp\$"} = 1;
	      }
	  }
	$title_done = join ('|',sort keys %title_done);
	$subtitle_done = join ('|',sort keys %subtitle_done);

	# Ein paar Zeichen Escapen
	$title_done =~ s/\?/\\\?/g;
	$title_done =~ s/\+/\\\+/g;
	$subtitle_done =~ s/\?/\\\?/g;
	$subtitle_done =~ s/\+/\\\+/g;

	# Now delete Timers in VDR that are already in the done-List
	my ($position, $timer, $active, $g, $title, $subtitle, $counter, @todel);
	$counter = 0;
	@list = GetSend ("LSTT");
	
	foreach $timer (@list)
	  {
#	    $timer =~ s/0x0d//g;
	    chomp $timer;
	    if ($timer =~ s/250-|250\s//)
	      {
		($position, $timer) = split (/\s/,$timer,2);
		# Split the : seperated values
		($active, $g, $g, $g, $g, $g, $g, $title, $subtitle) = split (/\:/,$timer,9);
		if ($active == 2)
		  {
		    # Title: "Shakespeare in Love"||Subtitle: "Romanze"
		    my ($ctitle, $csubtitle);
		    if ($subtitle && $subtitle =~ /^Title\:\s\"(.*)\"\|\|Subtitle\:\s\"(.*)\"/)
		      {
			$title = $1;
			$subtitle = $2;
			if ($subtitle)
			  {
			    my (@titles, @subtitles, $num, $hit);
			    undef $hit;
			    @titles = split (/\~/,$title);
			    @subtitles = split (/\~/,$subtitle);
			    foreach $num (0 .. $#titles)
			      {
				if ($titles[$num] =~ /$title_done/ && $subtitles[$num] =~ /$subtitle_done/)
				  {
				    $hit = 1;
				  }
				else
				  {
				    undef $hit;
				    last;
				  }
			      }

			    if ($hit)
			      {
				my ($result);
				print "Delete Timer: $title $subtitle\n" if ($debug & 4);
				$position -= $counter;
				($result) = GetSend ("DELT $position");
				print "Result: $result" if ($debug & 4);
				if ($result =~ /^250/)
				  {
				    $counter++;
				  }
			      }
			  }
		      }
		  }
	      }
	  }
      }
  }

# What should be recorded
sub inittorecord
  {
    my (@list, $list, $title, $subtitle, $description, $channel, $timeframe, $prio, $timer_title, $margin, $machine, @title_list, @subtitle_list, @description_list);
    my $counter = 0;
    open (FI,"$ENV{HOME}/.master-timer/torecord") or sub_die ("Can't open file \"torecord\"\n");
    @list = <FI>;
    close(FI);

    foreach $list (0 .. $#list)
      {
	chomp $list[$list];
	if ($list[$list] && !($list[$list] =~ /^\#/))
	  {
	    ($title, $subtitle, $description, $channel, $timeframe, $prio, $timer_title, $margin, $machine) = split (/\|/,$list[$list]);

	    # Accept torecord only if it is for the current machine
	    if ((!$machine && $currentVDR == 1) || $machine == $currentVDR)
	      {
		if ($title)
		  {
		    $title_torecord[$counter] = $title;
		    $title_list[$#title_list + 1] = $title;
		  }
		if ($subtitle)
		  {
		    $subtitle_torecord[$counter] = $subtitle;
		    $subtitle_list[$#subtitle_list + 1] = $subtitle;
		  }
		if ($description)
		  {
		    $description_torecord[$counter] = $description;
		    $description_list[$#description_list + 1] = $description;
		  }
		if ($channel)
		  {
		    my (@temp);
		    @temp = split (/\;/,$channel);
		    foreach (0 .. $#temp)
		      {
			$channel_torecord[$counter][$_] = $temp[$_];
		      }
		  }
		if ($timeframe)
		  {
		    $timeframe_torecord[$counter] = $timeframe;
		  }
		if ($prio)
		  {
		    $prio_torecord[$counter] = $prio;
		  }
		else
		  {
		    $prio_torecord[$counter] = $default_prio;
		  }
		if ($timer_title)
		  {
		    $timer_title_torecord[$counter] = $timer_title;
		  }
		if ($margin)
		  {
		    my ($start, $stop);
		    ($start, $stop) = split (/;/,$margin, 2);
		    $marginstart_torecord[$counter] = $start if ($start);
		    $marginstop_torecord[$counter] = $stop if ($stop);
		  }
		# Set Default-Margins if not margins defined
		$marginstart_torecord[$counter] = $marginstart if (!$marginstart_torecord[$counter]);
		$marginstop_torecord[$counter] = $marginstop if (!$marginstop_torecord[$counter]);
		$counter++;
	      }
	  }
      }

    $num_torecord = $counter - 1;

    $title_torecord = join ('|',@title_list);
    $subtitle_torecord = join ('|',@subtitle_list);
    $description_torecord = join ('|',@description_list);

    if (!$title_torecord)
      {
	$title_torecord = "^Dieseshierwirdgarantiertnieundnimmeraufirgendetwassinnvollesmatchen\$";
      }
    if (!$subtitle_torecord)
      {
	$subtitle_torecord = "^Dieseshierwirdgarantiertnieundnimmeraufirgendetwassinnvollesmatchen\$";
      }
    if (!$description_torecord)
      {
	$description_torecord = "^Dieseshierwirdgarantiertnieundnimmeraufirgendetwassinnvollesmatchen\$";
      }
  }

# Parse the "channels.conf" of VDR
sub initchannellist
  {
    my ($counter, $chan, $garbage, $card, @temp_channels, $temp, $i);

    @temp_channels = GetSend ("LSTC");

    foreach $i (0 .. $#temp_channels)
      {
	$temp = $temp_channels[$i];
#	$temp =~ s/\x0d//g;
	chomp $temp;

	if ($temp =~ s/250-|250\s//)
	  {
	    ($counter, $temp) = split (/\s/,$temp,2);
	    ($chan, $garbage,$garbage, $garbage, $garbage, $garbage, $garbage, $card, $garbage) = split (/\:/,$temp);
	    $channels[$counter] = $chan;
	    $channels{$chan}{number} = $counter;
	    $channels{$chan}{card} = $card;
	    $counter++;
	  }
      }
  }

sub initconfigfile
  {
    open (FI,"$ENV{HOME}/.master-timer/config") or return;
    while (<FI>)
      {
	s/\#.*//;
	chomp;
	if ($_)
	  {
	    my ($key, $value);
	    ($key, $value) = split (/\s+=\s+/);
	    if ($key =~ /^debug$/i)
	      {
		$debug = $value;
		print "Debug-Level = $value\n" if ($debug & 16);
	      }
	    elsif ($key =~ /^marginstart$/i)
	      {
		print "Marginstart = $value\n" if ($debug & 16);
		$marginstart = $value;
	      }
	    elsif ($key =~ /^marginstop$/i)
	      {
		print "Marginstop = $value\n" if ($debug & 16);
		$marginstop = $value;
	      }
	    elsif ($key =~ /^DVBCards$/i)
	      {
		print "DVB_Cards = $value\n" if ($debug & 16);
		$DVB_cards = $value;
	      }
	    elsif ($key =~ /^defaultprio$/i)
	      {
		print "Default Priority = $value\n" if ($debug & 16);
		$default_prio = $value;
	      }
	    elsif ($key =~ /^Dest$/i)
	      {
		print "Destination Host/IP:Port = $value\n" if ($debug & 16);
		@Dest = split (/\s+/,$value);
	      }
	    elsif ($key =~ /^jointimers$/i)
	      {
		print "Join Timers = $value\n" if ($debug & 16);
		$jointimers = $value;
	      }
	    else
	      {
		print "Unkown Key: \"$key\" with Value: \"$value\"\n";
	      }
	  }
      }
    print "End Config\n" if ($debug & 16);
  }

sub init
  {
    &initconfigfile();
    &initsocket();
    &initmovie();
    &initblacklist();
    &initdone();
    &initchannellist();
    &initepgdata();
    &inittorecord();
  }
