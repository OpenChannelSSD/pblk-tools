#!/usr/bin/env bash

function pblk_create_dd_remove () {

# E.g. "nvme0n1"
DEV_NAME=$1
if [ -z "$DEV_NAME" ]; then
	echo "# FAILED: dev_name($DEV_NAME)"
	exit 1
fi
DEV_PATH="/dev/$DEV_NAME"

# E.g. "pblk0"
PBLK_NAME=$2
if [ -z "$PBLK_NAME" ]; then
	echo "#FAILED: pblk_name($PBLK_NAME)"
	exit 1
fi
PBLK_PATH="/dev/$PBLK_NAME"

BLUN=$3
if [ -z "$BLUN" ]; then
	echo "#FAILED: blun($BLUN)"
	exit 1
fi

ELUN=$4
if [ -z "$ELUN" ]; then
	echo "#FAILED: elun($ELUN)"
	exit 1
fi

CREATE_OUT=$(nvme lnvm create -d $DEV_NAME -n $PBLK_NAME -t pblk -b $BLUN -e $ELUN -f)
if [ "$?" -ne 0 ]; then
	echo "# FAILED: creating instance"
	exit 1
fi

DD_OUT=$(dd if=/dev/zero of=$PBLK_PATH bs=1M count=64)
if [ "$?" -ne 0 ]; then
	echo "# FAILED: dd failed"
	exit 1
fi

RM_OUT=$(nvme lnvm remove -n $PBLK_NAME)
if [ "$?" -ne 0 ]; then
	echo "# FAILED: removing instance"
	exit 1
fi

}

pblk_create_dd_remove nvme0n1 pblk0 0 15
pblk_create_dd_remove nvme0n1 pblk0 8 15
