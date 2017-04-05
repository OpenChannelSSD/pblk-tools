#!/usr/bin/env bash

if [ $UID -ne 0 ] ; then
	echo "# FAILED: Must run as root / sudo"
	exit 1
fi

DEV_NAME="$1"
if [ -z "$DEV_NAME" ]; then
	echo "# FAILED: dev_name($DEV_NAME)"
	exit 1;
fi

TEST="$2"
if [ -z "$TEST" ]; then
	echo "# FAILED: test($TEST)"
	exit 1;
fi

source pblk_test.sh

MD_NAME="${DEV_NAME}.meta"
MD_PATH="/tmp/$MD_NAME"

case $TEST in
0)
  echo "# Running pblk_test_fs test($TEST)"
  pblk_test_fs $DEV_NAME pblk0 8 15
  ;;
1)
  echo "# Running pblk_test_fs test($TEST)"
  pblk_test_fs $DEV_NAME pblk1 80 111
  ;;
2)
  echo "# Running pblk_test_fs test($TEST)"
  pblk_test_fs $DEV_NAME pblk2 24 39
  ;;
9)
  echo "# Running pblk_test_fs test($TEST)"
  pblk_test_fs $DEV_NAME pblk0 8 15
  pblk_test_fs $DEV_NAME pblk1 80 111
  pblk_test_fs $DEV_NAME pblk2 24 39
  ;;
*)
  echo "# Unknown test($TEST)"
  exit 1
  ;;
esac

echo "# Reading meta to $MD_PATH"
nvm_pblk meta_dump $DEV_PATH 2>&1 | tee $MD_PATH

echo "# meta is available at $MD_PATH"
