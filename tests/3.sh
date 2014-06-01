#!/bin/sh

(
	runlock -d -f lock bash -c 'for i in $(seq 1 5); do echo $i; sleep 1; done'
	r=$?
	echo $r
	if [ $r -ne 0 ]; then
	   exit 1
        fi
) &
sleep 2
runlock -d -f lock bash -c 'for i in $(seq 1 5); do echo $i; sleep 1; done'
