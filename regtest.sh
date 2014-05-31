#!/bin/sh -x

SRCDIR=$(cd -P -- $(dirname -- "$0") && pwd -P)
TESTDIR=$SRCDIR/tests
TEMPDIR=$(mktemp -t -d cronutils.regtest-XXXXXX)
trap "popd; rm -rf $TEMPDIR" 0 INT QUIT ABRT PIPE TERM

pushd $SRCDIR

for s in $TESTDIR/*.sh; do
	test=$(basename $s .sh)	
	$s > $TEMPDIR/$test.out 2>/dev/null
	r=$?
	if [ $r -ne 0 ]; then
	  echo $test: FAIL - exit code $r
	  cat $TEMPDIR/$test.out
	  exit 1
        fi
	diff $TESTDIR/$test.out $TEMPDIR/$test.out > $TEMPDIR/$test.diff
	if [ $? -ne 0 ]; then
	  echo $test: FAIL - output match:
	  cat $TEMPDIR/$test.diff
	  exit 1
        fi
	echo $test: OK
done
