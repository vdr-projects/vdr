#!/usr/bin/perl

# getskyepg.pl: Get EPG data for Sky channels from the Internet
#
# Connects to a running VDR instance via SVDRP, gets the channel data
# for the Sky channels and connects to Internet web pages to extract the
# EPG data for these channels. The result is sent to VDR via SVDRP.
#
# See the README file for copyright information and how to reach the author.
#
# $Id: getskyepg.pl 1.3 2004/02/15 13:35:52 kls Exp $

use Getopt::Std;
use Time::Local;

$Usage = qq{
Usage: $0 [options]

Options: -c filename        channel config file name (default: channels.conf.sky)
         -d hostname        destination hostname (default: localhost)
         -p port            SVDRP port number (default: 2001)
         -S source          channel source (default: S28.2E)
         -D days            days to get EPG for (1..7, default: 2)
};

die $Usage if (!getopts("c:d:D:hp:S:") || $opt_h);

$Conf   = $opt_c || "channels.conf.sky";
$Dest   = $opt_d || "localhost";
$Port   = $opt_p || 2001;
$Source = $opt_S || "S28.2E";
$Days   = $opt_D || 2;

$SkyWebPage = "www.bleb.org/tv/data/listings";
$WGET = "/usr/bin/wget -q -O-";
$LOGGER = "/usr/bin/logger -t SKYEPG";

$DST = -3600; # Daylight Saving Time offset
$SecsInDay = 86400;

@Channels = ();

$idxSource = 0;
$idxNumber = 1;
$idxName = 2;

Error("days out of range: $Days") unless (1 <= $Days && $Days <= 7);

sub Log
{
  system("$LOGGER '@_'");
}

sub Error
{
  Log(@_);
  die "$0: @_\n";
}

sub GetChannels
{
  open(CHANNELS, $Conf) || Error("$Conf: $!");
  while (<CHANNELS>) {
        chomp;
        next if (/^#/);
        my @a = split(":");
        push(@Channels, [@a]) unless ($a[$idxName] eq "x");
        }
  close(CHANNELS);
}

GetChannels();

sub GetPage
{
  my $channel = shift;
  my $day = shift;
  $day--;
  my $url = "$SkyWebPage/$day/$channel.xml";
  Log("reading $url");
  my @page = split("\n", `$WGET '$url'`);
  Log("received " . ($#page + 1) . " lines");
  return @page;
}

sub StripWhitespace
{
  my $s = shift;
  $s =~ s/\s*(.*)\s*/$1/;
  $s =~ s/\s+/ /g;
  return $s;
}

sub Extract
{
  my $s = shift;
  my $t = shift;
  $s =~ /<$t>([^<]*)<\/$t>/;
  return StripWhitespace($1);
}

# In order to get the duration we need to buffer the last event:
$Id = "";
$Time = 0;
$Title = "";
$Subtitle = "";
$Desc = "";

sub GetEpgData
{
  my ($channel, $channelID) = @_;
  my $numEvents = 0;
  SVDRPsend("C $channelID");
  $Time = 0;
  for $day (1 .. $Days) {
      my $dt = 0;
      my @page = GetPage($channel, $day);
      my $data = "";
      for $line (@page) {
          chomp($line);
          if ($line =~ /<programme>/) {
             $data = "";
             }
          elsif ($line =~ /<\/programme>/) {
             my $title = Extract($data, "title");
             my $subtitle = Extract($data, "subtitle");
             my $desc = Extract($data, "desc");
             my $start = Extract($data, "start");
             # 'end' is useless, because it is sometimes missing :-(
             # my $end = Extract($data, "end");
             if (!$subtitle) {
                # They sometimes write all info into the description, as in
                # Episode: some description.
                # Why don't they just fill in the data correctly?
                my ($s, $d) = ($desc =~ /([^:]*)[:](.*)/);
                if ($s && $d) {
                   $subtitle = $s;
                   $desc = $d;
                   }
                }
             # 'start' and 'end' as time of day isn't of much use here, since
             # the page for one day contains data that actually belongs to the
             # next day (after midnight). Oh well, lets reconstruct the missing
             # information:
             $start = "0" . $start if (length($start) < 4);
             my ($h, $m) = ($start =~ /(..)(..)/);
             $dt = $SecsInDay if ($h > 12);
             # convert to time_t:
             my @gmt = gmtime;
             $gmt[0] = 0;  # seconds
             $gmt[1] = $m; # minutes
             $gmt[2] = $h; # hours
             $time = timegm(@gmt) + ($day - 1) * $SecsInDay + ($h < 12 ? $dt : 0);
             # comensate for DST:
             $time += $DST if (localtime($time))[8];
             # create EPG data:
             if ($Time) {
                $duration = $time - $Time;
                SVDRPsend("E $Id $Time $duration");
                SVDRPsend("T $Title");
                SVDRPsend("S $Subtitle");
                SVDRPsend("D $Desc");
                SVDRPsend("e");
                $numEvents++;
                }
             # buffer the last event:
             $Id = $time / 60 % 0xFFFF; # this gives us unique ids for every minute of over 6 weeks
             $Time = $time;
             $Title = $title;
             $Subtitle = $subtitle;
             $Desc = $desc;
             }
          else {
             $data .= $line;
             }
          }
      }
  SVDRPsend("c");
  Log("generated $numEvents EPG events");
}

sub ProcessEpg
{
  for (@Channels) {
      my $channel = @$_[$idxName];
      my $channelID = @$_[$idxSource];
      Log("processing channel $channel - $channelID");
      SVDRPsend("PUTE");
      SVDRPreceive(354);
      GetEpgData($channel, $channelID);
      SVDRPsend(".");
      SVDRPreceive(250);
      }
  Log("done");
}

#---------------------------------------------------------------------------
# TODO: make this a Perl module??? What about Error()???

use Socket;

$Timeout = 300; # max. seconds to wait for response

$SIG{ALRM} = sub { Error("timeout"); };
alarm($Timeout);

$iaddr = inet_aton($Dest)                   || Error("no host: $Dest");
$paddr = sockaddr_in($Port, $iaddr);

$proto = getprotobyname('tcp');
socket(SOCK, PF_INET, SOCK_STREAM, $proto)  || Error("socket: $!");
connect(SOCK, $paddr)                       || Error("connect: $!");
select(SOCK); $| = 1;
SVDRPreceive(220);
ProcessEpg();
SVDRPsend("QUIT");

sub SVDRPsend
{
  my $s = shift;
  print SOCK "$s\r\n";
}

sub SVDRPreceive
{
  my $expect = shift | 0;
  my @a = ();
  while (<SOCK>) {
        s/\s*$//; # 'chomp' wouldn't work with "\r\n"
        push(@a, $_);
        if (substr($_, 3, 1) ne "-") {
           my $code = substr($_, 0, 3);
           Error("expected SVDRP code $expect, but received $code") if ($code != $expect);
           last;
           }
        }
  return @a;
}

#---------------------------------------------------------------------------
