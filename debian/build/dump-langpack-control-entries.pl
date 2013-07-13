#!/usr/bin/perl

use strict;
use warnings;

my $lp_avail_desc;
my $lp_unavail_desc;

my %all;
my %shipped;

my $moz_language_list;

while (@ARGV) {
    my $arg = shift(@ARGV);
    if ($arg eq '-l') {
        $moz_language_list = 1;
    } else {
        die "Unknown argument '$arg'";
    }
}

if (not defined($moz_language_list)) {
    my $file;
    local $/=undef;
    open($file, "debian/control.langpacks") or die "Couldn't find control.langpacks";
    $lp_avail_desc = <$file>;

    open($file, "debian/control.langpacks.unavail") or die "Couldn't find control.langpacks.unavail";
    $lp_unavail_desc = <$file>;
}

open(my $all_file, "debian/config/locales.all") or die "Failed to open debian/config/locales.all";
open(my $shipped_file, "debian/config/locales.shipped") or die "Failed to open debian/config/locales.shipped";
while (<$all_file>) {
    chomp($_);
    /^([^:#]*):([^:]*)$/ && do {
        my $pkgname = $1;
        my $desc = $2;
        if ($desc eq "") { die "Malformed locales.all"; }
        $all{$pkgname} = $desc;
    }
}

while (<$shipped_file>) {
    chomp($_);
    /^([^:#]*):([^:]*)$/ && do {
        my $locale = $1;
        my $pkgname = $2;
        if ($pkgname eq "") { die "Malformed locales.shipped"; }
        $shipped{$pkgname} = 1;
    }
}
close($all_file);
close($shipped_file);

my $first = 1;

foreach my $pkg (sort(keys(%all))) {
    if (not defined($moz_language_list)) {
        my $entry = exists $shipped{$pkg} ? $lp_avail_desc : $lp_unavail_desc;
        $entry =~ s/\@LANGCODE\@/$pkg/g;
        $entry =~ s/\@LANG\@/$all{$pkg}/g;
        print $entry;
    } else {
        if ($first != 1) { print ","; }
        $first = 0;
        print "$pkg";
    }
}
