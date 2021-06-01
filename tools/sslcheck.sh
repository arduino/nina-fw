#!/bin/bash

while getopts "c:l:e" opt;do
	case $opt in
	c ) export CER_FILE="$OPTARG";;
	l ) export URL_LIST="$OPTARG";;
	e ) export SHOW_ERR=1;;
	* )
		echo "Unknown parameter."
		exit 1
		;;
	esac
done

if [ $# -eq 0 ] ; then
	echo "Usage: $(basename $0) [-c /path/to/certificate/file.pem] [-l path/to/url/list.txt]"
	echo
	echo " -c specify certificate file to test"
	echo " -l specify url list"
	echo " -e show curl errors in log"
	echo
	echo "Example:"
	echo " $(basename $0) -c roots.pem -l url_list.txt"
	exit 0
fi

export SHOW_ERR=${SHOW_ERR:-0}

echo
echo SHOW_ERR=$SHOW_ERR
echo

for i in $(cat $URL_LIST)
do
echo -n "$i "
# -s: silent
# -S: show error
# -m: max time
# --cacert: path to certificate pem file
# --capath: local certificate path
# --output: stdout output
if [ "$SHOW_ERR" -eq 1 ] ; then
	m=$(curl "$i" -s -S -m 60 --cacert $CER_FILE --capath /dev/null --output /dev/null --stderr -)
else
	curl "$i" -s -m 60 --cacert $CER_FILE --capath /dev/null --output /dev/null
fi
#curl --cacert roots.pem --trace-ascii log.log -K url_list.txt 
if [ $? -eq 0 ] ; then
echo -e "\e[32m PASS \e[39m"
else
echo -n -e "\e[31m FAIL \e[39m"
echo $m
fi
done
