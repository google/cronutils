#!/bin/sh
set -x
SRCDIR=$(cd -P -- $(dirname -- "$0") && pwd -P)
TESTDIR=$SRCDIR/tests
TEMPDIR=$(mktemp -t -d cronutils.regtest-XXXXXX)
trap "cd $SRCDIR; rm -rf $TEMPDIR" 0 INT QUIT ABRT PIPE TERM

cd $TEMPDIR

export PATH=$SRCDIR:$PATH

for s in $TESTDIR/*.sh; do
	test=$(basename $s .sh)	
	mkdir $TEMPDIR/$test
	cd $TEMPDIR/$test
	$s > $TEMPDIR/$test.out 2>$TEMPDIR/$test.err
	r=$?
	if [ $r -ne 0 ]; then
	  echo $test: FAIL - exit code $r
	  echo stdout
	  cat $TEMPDIR/$test.out
	  echo stderr
	  cat $TEMPDIR/$test.err
	  exit 1
        fi
	diff $TESTDIR/$test.out $TEMPDIR/$test.out > $TEMPDIR/$test.diff
	if [ $? -ne 0 ]; then
	  echo $test: FAIL - output match:
	  cat $TEMPDIR/$test.diff
	  exit 1
        fi
	echo $test: OK
	cd $TEMPDIR
done
