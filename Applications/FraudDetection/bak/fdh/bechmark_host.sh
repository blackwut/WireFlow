#!/bin/sh

make host HOSTMEM=1

APP=fdh
RUNTIME=3
SAMPLING=1024
KBUFFERING=8
SLEEP_TIME=5

TIMEOUT_TIME=$(($RUNTIME * 2))

for PAR in 1 2 3 4 5; do
    # for EXP        in 2 3  4  5  6   7   8   9   10   11   12   13    14    15    16; do
    # for BATCH_SIZE in 4 8 16 32 64 128 256 512 1024 2048 4096 8192 16384 32768 65536; do
    for EXP in `seq 7 16`; do
        BATCH_SIZE=$((1 << $EXP))
        echo "$i) ./bin/host kernels/$APP$PAR$PAR$PAR/hw/build/$APP.xclbin $RUNTIME $SAMPLING $PAR $PAR $KBUFFERING $KBUFFERING $BATCH_SIZE $BATCH_SIZE"
        timeout $TIMEOUT_TIME ./bin/host kernels/$APP$PAR$PAR$PAR/hw/build/$APP.xclbin $RUNTIME $SAMPLING $PAR $PAR $KBUFFERING $KBUFFERING $BATCH_SIZE $BATCH_SIZE
	    echo "Sleep"
	    for s in `seq 1 $SLEEP_TIME`; do
            echo " ."
            sleep 1
        done
    done
done
