#!/bin/sh

make host BENCHMARK=thr

TIMEOUT_TIME=30

HOST_NAME="host"
APP_TIME=10
SOURCE_BATCHES=2
SINK_BATCHES=2
SINK_BATCH_SIZE=32
SAMPLING_NUM=16

SLEEP_TIME=4

MODEL_PATH="/home/root/datasets/fd/model.txt" 
DATASET_PATH="/home/root/datasets/fd/credit-card.dat"
RESULT_PATH="/home/root/pdp/benchmarks/fd/fd_thr.csv"

for TRANSFER in "shared"; do
    for PAR in "1,1,1,1" "1,2,2,1" "1,4,4,1" "1,6,6,1"; do
        timeout -t $TIMEOUT_TIME ./$HOST_NAME 0 0 2 1024 2 1024 $MODEL_PATH $DATASET_PATH $PAR $TRANSFER 1 ""
        sleep $SLEEP_TIME
        for SOURCE_EXP in 2 3 4 5 6 7 8 10; do
            for i in `seq 10`; do
                timeout -t $TIMEOUT_TIME ./$HOST_NAME 0 0 $SOURCE_BATCHES $((1 << $SOURCE_EXP)) $SINK_BATCHES $SINK_BATCH_SIZE $MODEL_PATH $DATASET_PATH $PAR $TRANSFER $APP_TIME $RESULT_PATH $SAMPLING_NUM
                sleep $SLEEP_TIME
            done
        done
    done
done