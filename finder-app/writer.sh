#!/bin/bash

writefile=$1
writestr=$2

if [ $# -eq 0 ]
then
    echo "Usage: $0 writefile writestr"
    exit 1
fi

if [ $# -lt 2 ]
then
    echo "Error: too few arguments"
    exit 1
fi

if [ $# -gt 2 ]
then
    echo "Error: too many arguments"
    exit 1
fi

if [[ $writefile =~ "/" ]]
then
    mkdir -p "${writefile%/*}"
fi

echo $writestr > $writefile

if [ $? -ne 0 ]
then
    echo "Error: failed to write to ${writefile}"
    exit 1
fi
