#!/bin/sh

runalarm -d -t 5 /bin/bash -c 'for i in $(seq 1 7); do echo $i; sleep 1; done; echo "exited"'
r=$?
# Should be nonzero exit when killed, sigalarm
if [ $r -eq 142 ]; then
	exit 0
elif [ $r -eq 0 ]; then
	exit 1
else
	exit $r
fi
