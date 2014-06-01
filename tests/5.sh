#!/bin/sh

runstat -d -f foo runalarm -d -t 5 /bin/bash -c 'for i in $(seq 1 7); do echo $i; sleep 1; done; echo exited'
r=$?
if [ $r -ne 142 ]; then
	exit 1
fi

grep -q 'runalarm,exit_status,142' foo && exit 0

cat foo
exit 1
