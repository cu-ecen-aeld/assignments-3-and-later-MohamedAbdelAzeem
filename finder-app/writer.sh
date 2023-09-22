#!/bin/sh


if [ $# -gt  2 ]; then
	echo "Error : too many input paramters ! "
	exit 1
fi 

if [ $# -lt 2 ]; then
	echo "Error : missing input paramters ! "
	exit 1
fi 

WRITEFILE=$1 
WRITESTR=$2


if [ -z ${WRITEFILE} ]; then
	echo "Error : First input is not supplied !"
	exit 1
fi



if [ -z ${WRITESTR} ]; then
	echo "Error : Second input is not supplied !"
	exit 1
fi

mkdir -v -p $(dirname "$WRITEFILE")

if [ $? -ne 0  ]; then
	echo "Error : Failed create a dir !"
	exit 1
fi


touch $WRITEFILE

echo "$WRITESTR" > "$WRITEFILE"