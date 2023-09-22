#!/bin/sh

#finder.sh (assignment 1)


if [ $# -gt  2 ]; then
	echo "Error : too many input paramters ! "
	exit 1
fi 

if [ $# -lt 2 ]; then
	echo "Error : missing input paramters ! "
	exit 1
fi 

FILESDIR=$1 
SEARCHSTR=$2

if [ -z ${FILESDIR} ]; then
	echo "Error : First input is not supplied !"
	exit 1
fi

if [ ! -d ${FILESDIR} ]; then 
	echo "Error: the first argument is not a directory ! "
	exit 1 	
fi


if [ -z ${SEARCHSTR} ]; then
	echo "Error : Second input is not supplied !"
	exit 1
fi


FILE_COUNT=$(find $FILESDIR -type f | wc -l)

MATCHLINES=$(grep -r $SEARCHSTR $FILESDIR  | wc -l )

echo $(grep -r $SEARCHSTR $FILESDIR)

echo The number of files are $FILE_COUNT  and the number of matching lines are $MATCHLINES
