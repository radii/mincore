#!/bin/sh

# list all mapped files
# for each file, list all in-core pages
# map (file, page) -> (device, block offset, file, page)
# (remove files with no mapped pages...)
# sort based on (device, blockno)
# join the resulting list of (file, page) into (file, p1, p2, p3, ...) lists

find /proc/*/maps | xargs cat 2>/dev/null | \
    awk '/[a-f0-9]*-[a-f0-9]* .*\// && !/ \(deleted\)$/ { print substr($0, 74) }' | \
    perl -ne 'print if($x{$_}++==0)' | xargs mincore -flm | awk 'NF>1{print}'|\
    sudo xargs -L1 `which blockno` | awk '{printf("%s %10d %s %s\n", $3, $4, $1, $2)}' | \
    sort | awk '{print $3, $4}' | \
    perl -ne 'split; if($x eq $_[0]) {push @o, $_[1];}else
	    {printf("%s %s\n", $x, join " ",@o)if($x);@o=();$x=$_[0]}'
