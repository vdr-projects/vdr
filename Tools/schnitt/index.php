<html>
<head>
  <title>Schneiden</title>
</head>
<body bgcolor=#C0C0C0>
<?
if ($level == 0)
  {
    $dircount=0;
    $handle=opendir('/x1/video');
    while ($file = readdir($handle)) {
        if ($file != "." && $file != ".." && $file != "epg.data") { 
          $dir=$file;
          $dircount++;
        }
    }
  if ($dircount == 1) {
    $level=1;
    }
  else
  {
?>
<center><h1>Sender</h1></center>
<form action="index.php" method="post">
<input type=hidden name=level value="1">
<?
      $handle=opendir('/x1/video');
      while ($file = readdir($handle)) {
          if ($file != "." && $file != ".." && $file != "epg.data") { 
              echo "<input type=submit name=dir value=\"$file\">\n"; 
          }
      }
      closedir($handle); 
?>
</form>
<?
    }
  }
if ($level == 1)
  {
    $dircount=0;
    $handle=opendir("/x1/video/$dir");
    while ($file = readdir($handle)) {
        if ($file != "." && $file != "..") { 
          $dira="$dir/$file";
          $dircount++;
        } 
    }
    if ($dircount == 1) {
      $dir = $dira;
      $level = 2;
    }
    else
    {
?>
<form action="index.php" method="post">
<input type=hidden name=level value="2">
<?
      echo "<center><h1>Filme/Serien fuer den Sender $dir</h1></center>";
      $handle=opendir("/x1/video/$dir");
      while ($file = readdir($handle)) {
          if ($file != "." && $file != "..") { 
              echo "<input type=submit name=dir value=\"$dir/$file\"><br>\n"; 
          } 
      }
      closedir($handle); 
?>
</form>
<?
    }
  }
if ($level == 2)
  {
if ($aindex)
  $index = $aindex;
else if (!$index)
  $index = 0;

if ($dir)
  chdir ("/x1/video/$dir");

switch ($cindex) {
  case "-10000": 
      if ($index >=10000)
        $index -= 10000;
	break;
  case "-4000": 
      if ($index >=4000)
        $index -= 4000;
	break;
  case "-2000": 
      if ($index >=2000)
        $index -= 2000;
	break;
  case "-1000": 
      if ($index >=1000)
        $index -= 1000;
	break;
  case "-500": 
      if ($index >=500)
        $index -= 500;
	break;
  case "-100": 
      if ($index >=100)
        $index -= 100;
	break;
  case "Vorheriges I-Frame":
        $pindex = $index - 1;
        $fp = popen ("/usr/local/bin/my/getpreviframe.pl $pindex","r");
        $i = fgets($fp,1000);
        $index = chop ($i);
        pclose ($fp);
        break;
  case "Naechstes I-Frame":
        $index ++;
        break;
  case "+100":
        $index += 100;
	break;
  case "+500":
        $index += 500;
	break;
  case "+1000":
        $index += 1000;
	break;
  case "+2000":
        $index += 2000;
	break;
  case "+4000":
        $index += 4000;
	break;
  case "+10000":
        $index += 10000;
	break;
  }

if ($test)
  {
    $fp = popen ("/usr/local/bin/my/schnitt3.pl $index","r");
    $i = fgets($fp,1000);
    pclose ($fp);
    $index = chop ($i);
  }

if ($name)
  {
    $fp = fopen ("cut","w");
    fputs ($fp,"$name\n");
    fclose ($fp);
  }

if ($cut)
  {
    $fp = fopen ("cut","a");
    fputs ($fp,"$index\n");
    fclose ($fp);
  }

$fp = popen ("/usr/local/bin/my/schnitt.pl $index","r");
$i = fgets($fp,1000);
pclose ($fp);
$index = chop ($i);

system ("/usr/local/bin/my/dumpframe /x2/temp/bild.m2v");
system ("mv output.ppm /x2/temp");
system ("touch /x2/temp/newpic");
system ("killall sleep");
?>
<form action="index.php" method="post">
<input type=hidden name=level value="2">
<input type=hidden name=dir value="<?=$dir?>">
<input type=hidden name=index value=<?=$index?>>
<table width=90% align=center>
<tr>
<td><h1>Index <?=$index?></h1></td>
<td><h1>Dir: <?=$dir?></h1></td>
</tr>
</table>
<table width=80% align=center>
<tr>
<td><input type=submit name=cindex value="-10000"></td>
<td><input type=submit name=cindex value="-4000"></td>
<td><input type=submit name=cindex value="-2000"></td>
<td><input type=submit name=cindex value="-1000"></td>
<td><input type=submit name=cindex value="-500"></td>
<td><input type=submit name=cindex value="-100"></td>
<td><input type=submit name=cindex value="Vorheriges I-Frame"></td>
<td><input type=submit name=cindex value="Naechstes I-Frame"</td>
<td><input type=submit name=cindex value="+100"></td>
<td><input type=submit name=cindex value="+500"></td>
<td><input type=submit name=cindex value="+1000"></td>
<td><input type=submit name=cindex value="+2000"></td>
<td><input type=submit name=cindex value="+4000"></td>
<td><input type=submit name=cindex value="+10000"></td>
</tr>
</table>
<table>
<tr>
<td>Absoluter Index: <input type=text name=aindex size=6></td>
<td><input type=submit name=test value="Schnitt-Test"></td>
<td><input type=submit name=cut value="Mark"></td>
</form>
<form action="index.php" method="post">
<input type=hidden name=level value="2">
<input type=hidden name=dir value="<?=$dir?>">
<input type=hidden name=index value=<?=$index?>>
<td>Titel: <input type=text name=name size=50 maxlength=255></td>
</form>
</tr>
</table>
<?
}
?>
</body>
</html>
