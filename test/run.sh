#!/usr/bin/env bash

DEV_NAME="nvme0n1"
DEV_PATH="/dev/$DEV_NAME"

PBLK_NAME="pblk0"
PBLK_PATH="/dev/$PBLK_NAME"

# Grab geometry
NCHANNELS=$(nvm_dev geo $DEV_PATH | grep "nchannels" | cut -d ":" -f 2)
NLUNS=$(nvm_dev geo $DEV_PATH | grep "nluns" | cut -d ":" -f 2)
TLUNS=$(( $NCHANNELS * $NLUNS ))
ELUN=$(( $TLUNS - 1 ))

# Create pblk instance if it does not exist
if [ ! -e $PBLK_PATH ]; then
	echo "# Creating pblk instance"
	sudo nvme lnvm create -d $DEV_NAME -n $PBLK_NAME -t pblk -b 0 -e $ELUN
fi

nvm_pblk mdck $DEV_PATH
