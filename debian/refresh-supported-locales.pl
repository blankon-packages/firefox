#!/usr/bin/perl

use strict;
use warnings;
use Cwd;

my $moz_supported_file;
my $lpom_dir;

my %blacklist;
my %locale2pkgname;
my %languages;

my %oldsupported;

my $dir=getcwd;
chomp($dir);

my $file;

while (@ARGV) {
    my $arg = shift(@ARGV);
    if ($arg eq '-s') {
        $moz_supported_file = shift(@ARGV);
    } elsif ($arg eq '-l') {
        $lpom_dir = shift(@ARGV);
    } else {
        die "Unknown argument '$arg'";
    }
}

(defined($moz_supported_file)) || die "Need to specify a supported language list";

if (defined($lpom_dir)) {
    my $lang_file = "$lpom_dir/maps/languages";
    my $map_file = "$lpom_dir/maps/locale2pkgname";
    my $variant_file = "$lpom_dir/maps/variants";

    open($file, $lang_file);
    while (<$file>) {
        my $line = $_;
        chomp($line);
        my $langcode = $line;
        my $lang = $line;
        $langcode =~ s/([^:]*):*([^:]*)/$1/;
        $lang =~ s/([^:]*):*([^:]*)/$2/;
        if ($lang ne "") { $languages{$langcode} = $lang; }
    }
    close($file);

    open($file, $map_file);
    while (<$file>) {
        my $line = $_;
        chomp($line);
        my $langcode = $line;
        my $pkgname = $line;
        $langcode =~ s/([^:]*):*([^:]*)/$1/;
        $pkgname =~ s/([^:]*):*([^:]*)/$2/;
        if ($pkgname ne "") { $locale2pkgname{$langcode} = $pkgname; }
    }
    close($file);

    open($file, $variant_file);
    while (<$file>) {
        my $line = $_;
        chomp($line);
        my $langcode = $line;
        my $lang = $line;
        $langcode =~ s/([^:]*):*([^:]*)/$1/;
        $lang =~ s/([^:]*):*([^:]*)/$2/;
        if ($lang ne "") { $languages{$langcode} = $lang; }
    }
    close($file);
}

if (-e "$dir/debian/locales.shipped") {
    open($file, "$dir/debian/locales.shipped");
    while (<$file>) {
        if ((not $_ =~ /^$/) && (not $_ =~ /^#.*/)) {
            my $line = $_;
            chomp($line);
            my $langcode = $line;
            my $pkgname = $line;
            my $lang = $line;
            $langcode =~ s/([^:]*):*([^:]*):*([^:]*)/$1/;
            $pkgname =~ s/([^:]*):*([^:]*):*([^:]*)/$2/;
            $lang =~ s/([^:]*):*([^:]*):*([^:]*)/$3/;
            if (($pkgname eq "") || ($lang eq "")) { die "Malformed locales.shipped file"; }
            $languages{$pkgname} = $lang;
            $oldsupported{$pkgname} = 1;
            $locale2pkgname{lc($langcode)} = $pkgname;
        }
    }
    close($file);
}

if (-e "$dir/debian/locales.unavailable") {
    open($file, "$dir/debian/locales.unavailable");
    while (<$file>) {
        if ((not $_ =~ /^$/) && (not $_ =~ /^#.*/)) {
            my $line = $_;
            chomp($line);
            my $pkgname = $line;
            my $desc = $line;
            $pkgname =~ s/([^:]*):*([^:]*)/$1/;
            $desc =~ s/([^:]*):*([^:]*)/$2/;
            $oldsupported{$pkgname} = 1;
            if ($desc ne "") { $languages{$pkgname} = $desc; }
        }
    }
    close($file);
}

if (-e "$dir/debian/locales.blacklist") {
    open($file, "$dir/debian/locales.blacklist");
    while (<$file>) {
        if ((not $_ =~ /^$/) && (not $_ =~ /^#.*/)) {
            my $line = $_;
            chomp($line);
            $blacklist{$line} = 1;
        }
    }
    close($file);
}

my $have_language = 0;

open($file, $moz_supported_file);
open(my $outfile, ">$dir/debian/locales.shipped");
while (<$file>) {
    my $line = $_;
    chomp($line);
    my $langcode = $line;
    my $platforms = $line;
    $langcode =~ s/^([[:alnum:]\-]*)[[:space:]]*(.*)/$1/;
    $platforms =~ s/^([[:alnum:]\-]*)[[:space:]]*(.*)/$2/;
    if (($langcode eq "en-US") || (($platforms ne "") && (rindex($platforms, "linux") eq -1)) || (exists $blacklist{$langcode})) { next; }
    my $llangcode = lc($langcode);
    my $pkgname = $llangcode;
    if (exists $locale2pkgname{$llangcode}) { $pkgname = $locale2pkgname{$llangcode}; }
    if (not exists $languages{$pkgname}) {
        if ($pkgname eq $llangcode) { $pkgname =~ s/\-.*//; }
        if (not exists $languages{$pkgname}) { die "No description for $pkgname"; }
    }
    $have_language = 1;
    my $description = $languages{$pkgname};
    print $outfile "$langcode:$pkgname:$description\n";
    delete $oldsupported{$pkgname};
}

if ($have_language eq 0) { print $outfile "# Placeholder file for the list of shipped languages. Do not delete"; }
close($file);
close($outfile);

open($outfile, ">$dir/debian/locales.unavailable");
my @unavailable = keys(%oldsupported);
if (scalar(@unavailable) gt 0) {
    @unavailable = sort(@unavailable);
    foreach my $lang (@unavailable) {
        if (exists $languages{$lang}) {
            my $desc = $languages{$lang};
            print $outfile "$lang:$desc\n";
        } else { print $outfile "$lang\n"; }
    }
} else { print $outfile "# Placeholder file for the list of unavailable languages. Do not delete"; }
close($outfile);
