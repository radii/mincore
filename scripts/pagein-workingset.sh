#!/bin/sh

# read the input from get-workingset.sh and feed it to pagein(1)
xargs -L1 pagein
