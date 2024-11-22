#!/bin/bash

filesdir=$1
searchstr=$2

if [ $# -lt 2 ] || [ ! -d $filesdir ]; then
    echo "Usage: $0 <filesdir> <searchstr>"
    echo "Parameters:" 
    echo "<filesdir> : The directory where files are located." 
    echo "<searchstr> : The string to search in all dir-files and subdir-files."
    exit 1
fi

filescnt=$(find $filesdir -type f | wc -l)
linescnt=$(grep -r $searchstr $filesdir | wc -l)
echo "The number of files are $filescnt and the number of matching lines are $linescnt"
