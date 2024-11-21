#!/bin/sh

if [ $# -eq 0 ]
then
    echo "Usage: $0 filesdir searchstr"
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

if [ ! -d $1 ]
then
    echo "Error: ${1} is not a directory"
    exit 1
fi

X=$(grep -rl $2 $1 | wc -l)
Y=$(grep -r $2 $1 | wc -l)

echo "The number of files are ${X} and the number of matching lines are ${Y}"
