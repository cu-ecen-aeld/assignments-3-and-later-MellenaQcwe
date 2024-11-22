#!/bin/bash

writefile=$1
writestr=$2

if [ $# -lt 2 ]; then
    echo "Usage: $0 <writefile> <writestr>"
    echo "Parameters:" 
    echo "<writefile> : The Full path of the file to be created or overriden." 
    echo "<writestr> : The string to be written in <writefile>."
    exit 1
fi

mkdir -p $(dirname $writefile) && echo $writestr > $writefile

if [ $? -ne 0 ]; then
    echo "Writing $writestr into $writefile failed with rc $rc."
fi
