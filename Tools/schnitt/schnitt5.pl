#!/usr/bin/perl
require "/usr/local/bin/my/schnittcommon.pli";

open (INDEX,"index.vdr");
$index = $ARGV[0] - 15000;
&nextI;

$file1 = $file;

$index += 30000;
&nextI;

$file2 = $file;

print "$file1 $file2\n";

