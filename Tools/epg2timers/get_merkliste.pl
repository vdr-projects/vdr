#!/usr/bin/perl
# Create a user agent object

use HTML::Entities;
use HTML::Parser;
use LWP::UserAgent;
use IO::Handle;

STDOUT->autoflush(1);

$ua = new LWP::UserAgent;
$ua->agent("Mozilla/9.1 " . $ua->agent);
# $ua->proxy('http', 'http://localhost:8080/');

$filename = "merkliste.html";
$base_url = "http://www.tvtv.de";
# Hier das Bookmark von TVTV eintragen:
@files_to_fetch = ("/cgi-bin/bookmark.cgi?id=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

$num = 0;
$state = 0;

$p = HTML::Parser->new( api_version => 3,
                        start_h =>  [\&fparser_start, "tagname, attr"],
                        unbroken_text => 1 );

foreach $url (@files_to_fetch) {
   $nurl = $base_url . $url;
   print "Getting " . $nurl . "...\n";
   $req = new HTTP::Request GET => $nurl;
   $res = $ua->request($req);
   if ($res->is_success) {
      open (OUTFILE, ">" . $filename);
      print OUTFILE $res->content;
      close (OUTFILE);
      $p->parse ($res->content);
      $p->eof;
   } else {
      print "...FAILED\n";
   }
}
# Zielordner fuer die Speicherung der Merkliste:

print "...saved to 'merkliste.html'\n";
sub fparser_start {
   my($tagname, $attr_t) = @_;
   my(%attr) = %$attr_t;

   if ($tagname eq "frame") {
      if ($state == 1) {
         if (($attr{name} eq "frame_main") ||
             ($attr{name} eq "frame_nav") ||
             ($attr{name} eq "frame_nav_bottom")) {
            push @files_to_fetch, $attr{src};
         }
      }
      if ($state == 2) {
         if (($attr{name} eq "frame_content")) {
            push @files_to_fetch, $attr{src};
         }
      }
   }
   if ($tagname eq "a") {
      if ($attr{href} ne "") {
         $last_href = $attr{href};
         if ($state == 0) {
            push @files_to_fetch, $last_href;
            $state = 1;
         }
      }
   }
   if ($tagname eq "img") {
      if ($state == 1) {
         if ($attr{src} =~ /b_joblist/i) {
            $state = 2;
            push @files_to_fetch, $last_href;
         }
      }
   }
}


