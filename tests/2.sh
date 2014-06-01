#!/bin/sh

runalarm -d -t 5 runstat -d -f foo /bin/bash -c 'for i in $(seq 1 7); do echo $i; sleep 1; done; echo "exited"'
r=$?
if [ $r -ne 142 ]; then
	exit 1
fi

# runstat never got chance to write stats
if [ -e foo ]; then
	exit 1
fi
