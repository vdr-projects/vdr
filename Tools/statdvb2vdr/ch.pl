#!/usr/bin/perl

# Reads the file statdvb.dat produced by the Siemens windows 
# software (1.50), which contains the scanned channels of an
# DVB-C (-S). The file ist located in the windows directory.
#
# Output is suitable for VDR (channels.conf). Only tested for
# the cable version. Should work with slight modifications for
# the sat version.
#
# 8. März 2001 - Hans-Peter Raschke


# file structure derived from "DvbGlobalDef.h" of the Siemens
# DVB kit.

# typedef int TABLETYPE;
# enum TunStandard
# {
# 	PAL_BG,     //B/G stereo or mono
# 	PAL_I,      //I mono (with Nicam stereo)
# 	PAL_DK,     //D/K mono
# 	SECAM_L,    //L mono (with Nicam stereo)
# 	SECAM_LI,	//Secam L’ (with Nicam stereo)
# 	SECAM_DK,
# 	SECAM_BG,
# 	NTSC_M,
# 	DVB_C,
# 	DVB_S,
# 	DVB_T
# };
# 
# typedef struct TunProgDataTag //xx bytes+1string
# {
# 	int 	nNumber;				//logical number of the program
# 	DWORD   dwFrequency;				//frequency in khz
# 	CString 	csName;				//name of the program
# 	TunStandard	eStandard;			//standard of the program
# 	DWORD	dwExtraInfo;				//specific info, like teletext,reserved data
# 							//0x8 == external input 1-CVBS
# 							//0x10 == external input 2-Y/C
# 							//0x20 == scrambled Program stream
# 							//0x40 == ASTRA Sattable
# 							//0x80 == Eutelsat Sattable
# 							//0xC0 == Sattable from File
# 							//0x100 == Pulsed switch to other satellite dish
# 							//0x1000-0xF000 = Other Satellite Nr(if Sattable from File)
# //Digital only params beginning from here
# 	WORD ProgNr;					//DVB Nr for the prog (PAS related)
# 	WORD wTS_ID;					//Transport-Stream ID orig.
# 	WORD wNW_ID;					//Network ID orig.
# 	WORD wService_ID;				//Service /Programm Id
# 	BYTE nModulation;				//Modulation-Type QAM,QPSK,other
# 	BYTE nFEC_outerinner;				//outer(high nibble) and inner(low n.)
# 	DWORD dwSymbolrate;				//in symbol/s
# 	BOOL b22kHz;					//east or west(TRUE) position in Sat
# 	BOOL bVertical_pos;				//horizontal or vertical(TRUE) position in SAT
# 	BYTE nProgtype;					//type of service (e.g. tv, radio)
# 	WORD wVideo_PID;				//video-pid of the channel
# 	WORD wAudio_PID;				//audio-pid of the channel
# 	WORD wPMT_PID;					//PID of the associated PMT
# 	WORD wTxt_PID;					//teletext PID for the program
# 	WORD wSubtitling_PID;				//subtitling PID for the program
# 	WORD wData_PID;					//PID for data broadcast
# 	BYTE nIPFilter;					//filter for different ip's
# 	DWORD dwReserved1;				//Shows some extended Information LOWORD=DataBroadcast_Id, 
# 							//MSB showing Databroadcast, (HIWORD & 0xFF)=ComponentTag from the stream ident desc
# 	DWORD dwReserved2;				//reserved dword
# }DVBTunProgData;
# 

use strict;
use FileHandle;

# for a full dump
my @varNames	= ("nNumber",			# logical number of the program 
                   "dwFrequency",		# frequency in khz
	           "csName",			# name of the program
	           "eStandard",			# standard of the program
	  	   "dwExtraInfo",		# specific info, like teletext,reserved data
						# 0x8 == external input 1-CVBS
						# 0x10 == external input 2-Y/C
						# 0x20 == scrambled Program stream
						# 0x40 == ASTRA Sattable
						# 0x80 == Eutelsat Sattable
						# 0xC0 == Sattable from File
						# 0x100 == Pulsed switch to other satellite dish
						# 0x1000-0xF000 = Other Satellite Nr(if Sattable from File)
	           "ProgNr",			# DVB Nr for the prog (PAS related)
	           "wTS_ID",			# Transport-Stream ID orig.
	           "wNW_ID",			# Network ID orig.
	           "wService_ID",		# Service /Programm Id
	           "nModulation",		# Modulation-Type QAM,QPSK,other
	           "nFEC_outerinner",		# outer(high nibble) and inner(low n.)
	           "dwSymbolrate",		# in symbol/s
	           "b22kHz",			# east or west(TRUE) position in Sat
	           "bVertical_pos",		# horizontal or vertical(TRUE) position in SAT
	           "nProgtype",			# type of service (e.g. tv, radio)
	           "wVideo_PID",		# video-pid of the channel
	           "wAudio_PID",		# audio-pid of the channel
	           "wPMT_PID",			# PID of the associated PMT
	           "wTxt_PID",			# teletext PID for the program
	           "wSubtitling_PID",	  	# subtitling PID for the program
	           "wData_PID");		# PID for data broadcast

my @outVar = ("csName", 
              "dwFrequency", 
	      "bVertical_pos", 
	      "b22kHz", 
	      "dwSymbolrate",
	      "wVideo_PID",
	      "wAudio_PID",
	      "wTxt_PID",
	      "dwExtraInfo",
	      "ProgNr");

# channels that need a valid smartcard
my @addCrypted = ("Extreme Sport",
                  "Bloomberg",
                  "Fashion TV",
                  "BET ON JAZZ",
                  "LANDSCAPE",
                  "Einstein",
                  "Single TV");
	      
my @chNames	= ();				# list of scanned channels
my $camNo	= 1;				# number of CI/CAM to use
my %chData;					# all channel data
my $buff;					# input buffer
my $fh		= new FileHandle("$ARGV[0]")	or die "Datei $ARGV[0] nicht gefunden!";

binmode($fh);					# could be run on windows
$fh->seek(4, 0);				# skip id

my $chCnt	= 0;
while (!$fh->eof()) {
  $chCnt++;

  last 					if ($fh->read($buff, 7) != 7);
  my ($nNumber, 
      $dwFrequency,
      $sLen
     )		= unpack("SLC", $buff);
  
  last 					if ($fh->read($buff, $sLen) != $sLen);
  my ($csName)	= unpack("A$sLen", $buff);
  $csName	=~ s/:/./g;
  $csName	=~ s/^\s+//;
  $csName	=~ s/\s+$//;

  last 					if ($fh->read($buff, 54) != 54);
  my ($eStandard,
      $dwExtraInfo,
      $ProgNr,
      $wTS_ID,
      $wNW_ID,
      $wService_ID,
      $nModulation,
      $nFEC_outerinner,
      $dwSymbolrate,
      $b22kHz,
      $bVertical_pos,
      $nProgtype,
      $wVideo_PID,
      $wAudio_PID,
      $wPMT_PID,
      $wTxt_PID,
      $wSubtitling_PID,
      $wData_PID
     )		= unpack("LLSSSSCCLLLCSSSSSS", $buff);

  # some modifications for VDR  
  $dwFrequency 	 /= 1000;
  $bVertical_pos = $bVertical_pos ? "v" : "h";
  $dwSymbolrate	 /= 1000;
  $dwExtraInfo	 = ($dwExtraInfo == 32 || grep(($_ cmp $csName) == 0, @addCrypted)) ? $camNo : 0;

  my $x		= 1;
  my $orgName	= $csName;
  while (exists($chData{$csName})) {
    $csName	 = "$orgName" . "_$x";
    $x++;
  }
  push(@chNames, $csName);

  my %tmp = ("nNumber"		=> $nNumber,			
             "dwFrequency"	=> $dwFrequency,		
	     "csName"		=> $orgName,			
	     "eStandard"	=> $eStandard,			
	     "dwExtraInfo"	=> $dwExtraInfo,		
	     "ProgNr"		=> $ProgNr,			
	     "wTS_ID"		=> $wTS_ID,			
	     "wNW_ID"		=> $wNW_ID,			
	     "wService_ID"	=> $wService_ID,		
	     "nModulation"	=> $nModulation,		
	     "nFEC_outerinner"	=> $nFEC_outerinner,		
	     "dwSymbolrate"	=> $dwSymbolrate,		
	     "b22kHz"		=> $b22kHz,			
	     "bVertical_pos"	=> $bVertical_pos,		
	     "nProgtype"	=> $nProgtype,			
	     "wVideo_PID"	=> $wVideo_PID,		
	     "wAudio_PID"	=> $wAudio_PID,		
	     "wPMT_PID"		=> $wPMT_PID,			
	     "wTxt_PID"		=> $wTxt_PID,			
	     "wSubtitling_PID"	=> $wSubtitling_PID,	  	
	     "wData_PID"	=> $wData_PID);		
  $chData{$csName}	= {%tmp};
}

print STDERR "$chCnt channels found!\n";

# now we print the channels.conf	      
# crypted TV
print ":verschlüsselte Fernsehprogramme\n";
for my $n (@chNames) {
  my %tmp	= %{$chData{$n}};
  printChannel($chData{$n})		if ($tmp{"nProgtype"} == 1 && $tmp{"dwExtraInfo"});
}

# TV
print ":Fernsehprogramme\n";
for my $n (@chNames) {
  my %tmp	= %{$chData{$n}};
  printChannel($chData{$n})		if ($tmp{"nProgtype"} == 1 && !$tmp{"dwExtraInfo"});
}

# crypted radio
print ":verschlüsselte Radioprogramme\n";
for my $n (@chNames) {
  my %tmp	= %{$chData{$n}};
  printChannel($chData{$n})		if ($tmp{"nProgtype"} == 2 && $tmp{"dwExtraInfo"});
}

# radio
print ":Radioprogramme\n";
for my $n (@chNames) {
  my %tmp	= %{$chData{$n}};
  printChannel($chData{$n})		if ($tmp{"nProgtype"} == 2 && !$tmp{"dwExtraInfo"});
}

sub printChannel {
  my $p		= shift;
  my @tmp	= ();

  for my $n (@outVar) {
    push(@tmp, ${$p}{$n});
  }
  
  print join(":", @tmp), "\n";
}  
