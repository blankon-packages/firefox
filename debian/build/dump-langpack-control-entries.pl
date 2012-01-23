#!/usr/bin/perl

use strict;
use warnings;

my $ip_dir;
my $template_dir;

my $lp_avail_desc;
my $lp_unavail_desc;

my %all;
my %shipped;

while (@ARGV) {
    my $arg = shift(@ARGV);
    if ($arg eq '-i') {
        $ip_dir = shift(@ARGV);
    } elsif ($arg eq '-t') {
        $template_dir = shift(@ARGV);
    }
}

if (not defined($ip_dir) || not defined($template_dir)) { die "Need to specify a location for input and template data\n"; }

{
    my $file;
    local $/=undef;
    open($file, "$template_dir/control.langpacks") or die "Couldn't find control.langpacks";
    $lp_avail_desc = <$file>;

    open($file, "$template_dir/control.langpacks.unavail") or die "Couldn't find control.langpacks.unavail";
    $lp_unavail_desc = <$file>;
}

my $all_file;
my $shipped_file;

open($all_file, "$ip_dir/locales.all") or die "Failed to open $ip_dir/locales.all";
open($shipped_file, "$ip_dir/locales.shipped") or die "Failed to open $ip_dir/locales.shipped";
while (<$all_file>) {
    my $line = $_;
    chomp($line);
    $line =~ /^([^:#]*):([^:]*)$/ && do {
        my $pkgname = $1;
        my $desc = $2;
        if ($desc eq "") { die "Malformed locales.all"; }
        $all{$pkgname} = $desc;
    }
}

while (<$shipped_file>) {
    my $line = $_;
    chomp($line);
    $line =~ /^([^:#]*):([^:]*)$/ && do {
        my $locale = $1;
        my $pkgname = $2;
        if ($pkgname eq "") { die "Malformed locales.shipped"; }
        $shipped{$pkgname} = 1;
    }
}
close($all_file);
close($shipped_file);

my @pkglist = keys(%all);
@pkglist = sort(@pkglist);
foreach my $pkg (@pkglist) {
    my $desc = $all{$pkg};
    my $entry;
    $entry = exists $shipped{$pkg} ? $lp_avail_desc : $lp_unavail_desc;
    $entry =~ s/\@LANGCODE\@/$pkg/g;
    $entry =~ s/\@LANG\@/$desc/g;
    print $entry;
}
