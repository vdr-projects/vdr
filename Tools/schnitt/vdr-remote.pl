#!/usr/bin/perl -w

use strict;
use Socket;

my ($dest, $port, $iaddr, $paddr, $proto, $line);

$dest = "localhost";
$port = "2001";

$iaddr = inet_aton($dest)                   || Error("no host: $dest");
$paddr = sockaddr_in($port, $iaddr);

$proto = getprotobyname('tcp');
socket(SOCK, PF_INET, SOCK_STREAM, $proto)  || Error("socket: $!");
connect(SOCK, $paddr)                       || Error("connect: $!");
select (SOCK); $| = 1;
$a=<SOCK>;

for (;;)
  {
    open (FI,"/tmp/vdr-keys");
    while (<FI>)
      {
	chomp;
	print "$_\r\n";
	$a=<SOCK>;
      }
    close (FI);
  }

print "quit\r\n";
$a=<SOCK>;
close (SOCK)                                || Error("close: $!");

sub Error
{
  print STDERR "@_\n";
  exit 0;
}
