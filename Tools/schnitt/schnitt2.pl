#!/usr/bin/perl

require "/usr/local/bin/my/schnittcommon.pli";

if (!open (INDEX,"index.vdr"))
  {
    print "Error opening index.vdr";
    exit 1;
  }

$index = $ARGV[0];
&nextI;
$file1 = $file;
$offset1 = $offset;
$index = $ARGV[1];
&nextI;
$file2 = $file;
$offset2 = $offset;

if ($file1 == $file2)
  {
    $count = $offset2 - $offset1;
    $cond = 0;
    $size = 1024*1024;
    $fi = sprintf ("%03d.vdr",$file);
    open (FI,$fi);
    sysseek (FI,$offset1,0);
    while ($cond == 0)
      {
	if ($count > $size)
	  {
	    $read = sysread (FI,$data,$size);
            print $data;
	    $count -= $size;
	  }
	else
	  {
	    $read = sysread (FI,$data,$count);
            print $data;
	    $cond = 1;
	  }
      }
  }
else
  {
    $count = $offset2;
    $cond = 0;
    $read = $size = 1024*1024;
    $fi = sprintf ("%03d.vdr",$file1);
    open (FI,$fi);
    sysseek (FI,$offset1,0);
    while ($read == $size)
      {
	$read = sysread (FI,$data,$size);
        print $data;
      }
    close (FI);

    $file1++;
    while ($file1 != $file2)
      {
        $fi = sprintf ("%03d.vdr",$file1);
        open (FI,$fi);
        $read = 1024*1024;        
        while ($read == $size)
          {
            $read = sysread (FI,$data,$size);
            print $data;
          }
        close (FI);
        $file1++;
      }

    $fi = sprintf ("%03d.vdr",$file2);
    open (FI,$fi);
    while ($cond == 0)
      {
	if ($count > $size)
	  {
	    $read = sysread (FI,$data,$size);
            print $data;
	    $count -= $size;
	  }
	else
	  {
	    $read = sysread (FI,$data,$count);
            print $data;
	    $cond = 1;
	  }
      }
  }
