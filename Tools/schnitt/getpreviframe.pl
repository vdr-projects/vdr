#!/usr/bin/perl

require "/usr/local/bin/my/schnittcommon.pli";

if (!open (INDEX,"index.vdr"))
  {
    exit 1;
  }
$index = $oindex = $ARGV[0];
if ($index > 0)
{
  &prevI;
  if ($oindex != $index)
    {
      print "$index\n";
    }
  else
    {
      print "$oindex\n";
    }
}
else
{
  print "0\n";
}
