#!/bin/sh


runstat -d -f foo bash -c 'for i in $(seq 1 5); do echo $i; done; exit 5'
r=$?

if [ $r -ne 5 ]; then
	exit 1
fi

grep -q 'bash,exit_status,5' foo && exit 0

cat foo
exit 1
