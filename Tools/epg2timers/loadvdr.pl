#!/usr/bin/perl
#
# 0.01 loadvdr (peter)
# 0.02 delete old entries before updating (peter) 
# 0.03 dumped Net::Telnet because of lost connections
#
# please submit diffs to petera@gmx.net
#
# ./epg2timers < merkliste.html | perl -w loadvdr.pl
#
#

use Socket;
use Getopt::Std;

@resp = ();

$Dest = "localhost";
$Port = 2001;

$Timeout = 10; # max. seconds to wait for response

$SIG{ALRM} = sub { Error("timeout"); };
alarm($Timeout);

$iaddr = inet_aton($Dest)                   || Error("no host: $Dest");
$paddr = sockaddr_in($Port, $iaddr);

$proto = getprotobyname('tcp');
socket(SOCK, PF_INET, SOCK_STREAM, $proto)  || Error("socket: $!");
connect(SOCK, $paddr)                       || Error("connect: $!");
select(SOCK); $| = 1;
Receive_void();

Send("lstt");

foreach $item (reverse @resp){
  if ($item =~ /^250.(\d{1,2}).*\(epg2timers\)/) {
    Send_void("DELT $1");
  }
}

while (defined ($line = <STDIN>)) {
  chomp $line;
  Send_void("UPDT $line");
}

Send("quit");
close(SOCK)                                 || Error("close: $!");



sub Send
{
  my $cmd = shift || Error("no command to send");
  print SOCK "$cmd\r\n";
  Receive();
}

sub Send_void
{
  my $cmd = $_[0];
  print SOCK "$cmd\r\n";
  Receive_void();
}

sub Receive
{
  while (<SOCK>) {
        chomp;
        push @resp,$_;
        last if substr($_, 3, 1) ne "-";
        }
}

sub Receive_void
{
  while (<SOCK>) {
        last if substr($_, 3, 1) ne "-";
        }
}

sub Error
{
  print STDERR "@_\n";
  close(SOCK);
  exit 0;
}

