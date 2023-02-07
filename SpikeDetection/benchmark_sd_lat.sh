#!/bin/sh

make host BENCHMARK=lat

TIMEOUT_TIME=30

HOST_NAME="host"
APP_TIME=10
SOURCE_BATCHES=2
SINK_BATCHES=2
SAMPLING_NUM=16

SLEEP_TIME=4

DATASET_PATH="/home/root/datasets/sd/sensors.dat"
RESULT_PATH="/home/root/pdp/benchmarks/sd/sd_lat.csv"

for TRANSFER in "copy" "hybrid" "shared"; do
    for PAR in "1,1,1,1" "2,1,1,1"; do
        timeout -t $TIMEOUT_TIME ./$HOST_NAME 0 0 2 1024 2 1024 $DATASET_PATH $PAR $TRANSFER 1 ""
        sleep $SLEEP_TIME
        for SOURCE_EXP in 4 5 6 7 8 9 10 11 12; do
            for SINK_EXP in 8 10; do
                for i in `seq 10`; do
                    timeout -t $TIMEOUT_TIME ./$HOST_NAME 0 0 $SOURCE_BATCHES $((1 << $SOURCE_EXP)) $SINK_BATCHES $SINK_BATCH_SIZE $DATASET_PATH $PAR $TRANSFER $APP_TIME $RESULT_PATH $SAMPLING_NUM
                    sleep $SLEEP_TIME
                done
            done
        done
    done
done
