#!/usr/bin/perl

# getskyepg.pl: Get EPG data from Sky's web pages
#
# Connects to a running VDR instance via SVDRP, gets the channel data
# for the Sky channels and connects to Sky's web pages to extract the
# EPG data for these channels. The result is sent to VDR via SVDRP.
#
# See the README file for copyright information and how to reach the author.
#
# $Id: getskyepg.pl 1.2 2003/04/02 16:21:47 kls Exp $

use Getopt::Std;
use Time::Local;

$Usage = qq{
Usage: $0 [options]

Options: -d hostname        destination hostname (default: localhost)
         -p port            SVDRP port number (default: 2001)
         -S source          channel source (default: S28.2E)
         -D days            days to get EPG for (1..7, default: 2)
};

die $Usage if (!getopts("d:D:hp:S:") || $opt_h);

$Dest   = $opt_d || "localhost";
$Port   = $opt_p || 2001;
$Source = $opt_S || "S28.2E";
$Days   = $opt_D || 2;

$SkyWebPage = "www.ananova.com/tv/frontpage.html";
$WGET = "/usr/bin/wget -q -O-";
$LOGGER = "/usr/bin/logger -t SKYEPG";

$DST = -3600; ##XXX TODO find out whether DST is active!
$SecsInDay = 86400;

$MaxFrequency = 1000;
$idxName = 0;
$idxFrequency = 1;
$idxSource = 3;
$idxSid = 9;

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
  SVDRPsend("LSTC");
  my @channels = ();
  for (SVDRPreceive(250)) {
      my @a = split(':', substr($_, 4));
      if ($a[$idxSource] eq $Source && $a[$idxFrequency] < $MaxFrequency) {
         push(@channels, [@a]);
         }
      }
  return @channels;
}

sub GetPage
{
  my $channel = shift;
  my $day = shift;
  my $url = "$SkyWebPage?c=$channel&day=day$day";
  Log("reading $url");
  my @page = split("\n", `$WGET '$url'`);
  Log("received " . ($#page + 1) . " lines");
  return @page;
}

# In order to get the duration we need to buffer the last event:
$Id = "";
$Time = 0;
$Title = "";
$Episode = "";
$Descr = "";

sub GetEpgData
{
  my ($channel, $channelID) = @_;
  my $numEvents = 0;
  SVDRPsend("C $channelID");
  $Time = 0;
  for $day (1 .. $Days) {
      my $dt = 0;
      my $ap = "";
      my @page = GetPage($channel, $day);
      for $line (@page) {
          if ($line =~ /^<\/tr><tr /) {
             # extract information:
             my ($time, $title, $episode, $descr) = ($line =~ /^.*?<b>(.*?)<\/b>.*?<b>(.*?)<\/b> *(<i>.*?<\/i>)? *(.*?) *<\/small>/);
             my ($h, $m, $a) = ($time =~ /([0-9]+)\.([0-9]+)(.)m/);
             # handle am/pm:
             $dt = $SecsInDay if ($ap eq "p" && $a eq "a");
             $ap = $a;
             $h += 12 if ($a eq "p" && $h < 12);
             $h -= 12 if ($a eq "a" && $h == 12);
             # convert to time_t:
             my @gmt = gmtime;
             $gmt[0] = 0;  # seconds
             $gmt[1] = $m; # minutes
             $gmt[2] = $h; # hours
             $time = timegm(@gmt) + ($day - 1) * $SecsInDay + $dt + $DST;
             # create EPG data:
             if ($Time) {
                $duration = $time - $Time;
                SVDRPsend("E $Id $Time $duration");
                SVDRPsend("T $Title");
                SVDRPsend("S $Episode");
                SVDRPsend("D $Descr");
                SVDRPsend("e");
                $numEvents++;
                }
             # buffer the last event:
             $Id = $time / 60 % 0xFFFF; # this gives us unique ids for every minute of over 6 weeks
             $Time = $time;
             ($Title = $title)     =~ s/<[^>]+>//g;
             ($Episode = $episode) =~ s/<[^>]+>//g;
             ($Descr = $descr)     =~ s/<[^>]+>//g;
             }
          }
      }
  SVDRPsend("c");
  Log("generated $numEvents EPG events");
}

sub ProcessEpg
{
  Log("getting Sky channel definitions");
  my @channels = GetChannels();
  Error("no Sky channels found") unless @channels;
  Log("found " . ($#channels + 1) . " channels");
  for (@channels) {
      my $channel = @$_[$idxSid];
      my $channelID = "@$_[$idxSource]-0-@$_[$idxFrequency]-$channel";
      Log("processing channel @$_[0]");
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
