#!/usr/bin/perl

# Convert 'summary.vdr' files to 'info.vdr'
#
# Converts all 'summary.vdr' files in the video directory to the
# 'info.vdr' format as used from VDR version 1.3.25 upward.
#
# Usage: summary2info.pl /video
#
# See the main source file 'vdr.c' for copyright information and
# how to reach the author.
#
# $Id: summary2info.pl 1.3 2005/06/04 11:33:09 kls Exp $

$VideoDir = $ARGV[0] || die "please provide the name of the video directory\n";

@SummaryFiles = `find "$VideoDir" -name summary.vdr`;

for $SummaryFile (@SummaryFiles) {
    chomp($SummaryFile);
    print STDERR "converting $SummaryFile...";
    open(F, $SummaryFile) || die "$SummaryFile: $!\n";
    $line = 0;
    @data = ();
    while (<F>) {
          chomp;
          if ($_ || $line > 1) {
             $data[$line] .= '|' if ($data[$line]);
             $data[$line] .= $_;
             }
          else {
             $line++;
             }
          }
    close(F);
    if ($line == 1) {
       $data[2] = $data[1];
       $data[1] = "";
       }
    ($InfoFile = $SummaryFile) =~ s/summary\.vdr$/info.vdr/;
    open(F, ">$InfoFile") || die "$InfoFile: $!\n";
    print F "T $data[0]\n" if ($data[0]);
    print F "S $data[1]\n" if ($data[1]);
    print F "D $data[2]\n" if ($data[2]);
    close(F);
    print STDERR "done.\n";
    }
