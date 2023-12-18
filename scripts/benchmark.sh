#!/bin/bash

# APPLICATION: "sd", "fd" or "ffd"
# APPLICATION_SHORTNAME: "sds", "sdlc", "sdlh", "sdtc", "sdth", "fds", "fdc", "fdh", "ffds", "ffdc", "ffdh",
# APPLICATION_FULLNAME: "SpikeDetection", "FraudDetection", or "FraudFreqDetection"
# APPLICATION_DIR: "../Applications/SpikeDetection", "../Applications/FraudDetection", or "../Applications/FraudFreqDetection"
# APP

RUNS=1                         # number of runs
RUN_TIME=60                    # execution time in seconds
SAMPLING_RATE=1024             # samples per second
KBUFFERS=8                     # number of buffers for K-Buffering
SLEEP_TIME=5                   # seconds to sleep between runs
TIMEOUT_TIME=$((3 * RUN_TIME)) # seconds to wait for timeout before killing

function compile_host() {
    local HOST_FLAGS=""
    if [ "$TRANSFER" == "HOST" ]; then
        HOST_FLAGS="$HOST_FLAGS HOSTMEM=1"
    fi

    if [ "$BENCHMARK" == "LATENCY" ]; then
        HOST_FLAGS="$HOST_FLAGS MEASURE_LATENCY=1"
    fi

    echo "cd $APPDIR"
    cd $APPDIR

    echo "make host $HOST_FLAGS"
    if [ "$DRY_RUN" == "1" ]; then
        cd -
        return
    fi

    make host $HOST_FLAGS
    cd -
}

function run_benchmark() {
    cd $APPDIR
    for PAR in `seq 1 $MAX_PAR`; do
        #      2^    10   11   12   13    14    15    16
        #          1024 2048 4096 8192 16384 32768 65536
        for EXP in 10 11 12 13 14 15 16; do
            for i in `seq 1 $RUNS`; do
                BATCH_SIZE=$((2**$EXP))
                KERNEL_DIR="$APPLICATION_SHORTNAME"
                for i in `seq 1 $OPERATORS`; do
                    KERNEL_DIR="$KERNEL_DIR$PAR"
                done

                echo "timeout $TIMEOUT_TIME ./bin/host kernels/$KERNEL_DIR/hw/build/$APPLICATION_SHORTNAME.xclbin $RUN_TIME $SAMPLING_RATE $PAR $PAR $KBUFFERS $KBUFFERS $BATCH_SIZE $BATCH_SIZE"
                if [ "$DRY_RUN" == "1" ]; then
                    continue
                fi
                timeout $TIMEOUT_TIME ./bin/host kernels/$KERNEL_DIR/hw/build/$APPLICATION_SHORTNAME.xclbin $RUN_TIME $SAMPLING_RATE $PAR $PAR $KBUFFERS $KBUFFERS $BATCH_SIZE $BATCH_SIZE

                echo -n "Sleeping"
                for s in `seq 1 $SLEEP_TIME`; do
                    echo -n "."
                    sleep 1
                done
                echo ""
            done
        done
    done
    cd -
}

function run_benchmark_skeleton() {
    cd $APPDIR
    for PAR in `seq 1 $MAX_PAR`; do
        for i in `seq 1 $RUNS`; do
            echo "timeout $TIMEOUT_TIME ./bin/host $PAR"
            if [ "$DRY_RUN" == "1" ]; then
                continue
            fi
            timeout $TIMEOUT_TIME ./bin/host $PAR

            echo -n "Sleeping"
            for s in `seq 1 $SLEEP_TIME`; do
                echo -n "."
                sleep 1
            done
            echo ""
        done
    done
    cd -
}

while getopts 'a:t:b:r:snyh' flag; do
  case "${flag}" in
    a) APPLICATION="${OPTARG}" ;;
    t) TRANSFER="${OPTARG}" ;;
    b) BENCHMARK="${OPTARG}" ;;
    r) RUNS="${OPTARG}" ;;
    s) SKELETON=1 ;;
    n) DRY_RUN=1 ;;
    y) SKIP_CONFIRMATION=1 ;;
    h) echo "Usage: $0 [-a APPLICATION] [-t TRANSFER] [-b BENCHMARK] [-r RUNS] [-s] [-n] [-h]"
       echo "  -a APPLICATION: 'sd', 'fd' or 'ffd'"
       echo "  -t TRANSFER: 'COPY' or 'HOST'"
       echo "  -b BENCHMARK: 'latency' or 'throughput'"
       echo "  -r RUNS: number of runs"
       echo "  -s: skeleton version"
       echo "  -n: dry run, do not compile"
       echo "  -y: skip confirmation"
       echo "  -h: show this help"
       exit 0 ;;
    *) echo "Unexpected option ${flag}" >&2; exit 1 ;;
  esac
done

OPERATORS=4

# check if APPLICATION is "sd", "fd" or "ffd'
if [ "$APPLICATION" != "sd" ] && [ "$APPLICATION" != "fd" ] && [ "$APPLICATION" != "ffd" ]; then
    echo "Application must be 'sd', 'fd' or 'ffd'!"
    exit 1
fi

# set APPLICATION_FULLNAME ("SpikeDetection", "FraudDetection", or "FraudFreqDetection")
if [ "$APPLICATION" == "sd" ]; then
    APPLICATION_FULLNAME="SpikeDetection"
    OPERATORS=4
elif [ "$APPLICATION" == "fd" ]; then
    APPLICATION_FULLNAME="FraudDetection"
    OPERATORS=3
else
    APPLICATION_FULLNAME="FraudFreqDetection"
    OPERATORS=4
fi

# check if TRANSFER is "COPY" or "HOST" (case insensitive)
TRANSFER=$(echo $TRANSFER | tr '[:lower:]' '[:upper:]')
if [[ "$SKELETON" != 1 && "$TRANSFER" != "COPY" && "$TRANSFER" != "HOST" ]]; then
    echo "Transfer must be 'COPY' or 'HOST'!"
    exit 1
fi
TRANSFER_CHAR=$(echo $TRANSFER | head -c 1| tr '[:upper:]' '[:lower:]')

# check if BENCHMARK is "latency" or "throughput" (case insensitive) only when APPLICATION is SpikeDetection
BENCHMARK=$(echo $BENCHMARK | tr '[:lower:]' '[:upper:]')
if [ "$APPLICATION" == "sd" ]; then
    if [[ "$SKELETON" != 1 && "$BENCHMARK" != "LATENCY" && "$BENCHMARK" != "THROUGHPUT" ]]; then
        echo "Benchmark must be 'LATENCY' or 'THROUGHPUT'!"
        exit 1
    fi

    if [ "$BENCHMARK" == "LATENCY" ]; then
        BENCHMARK_CHAR="l"
    else
        BENCHMARK_CHAR="t"
    fi
fi

SKELETON_CHAR=""
if [ "$SKELETON" == "1" ]; then
    SKELETON_CHAR="s"
fi

MAX_PAR=6
if [ "$TRANSFER" == "HOST" ]; then
    MAX_PAR=5
fi

BASEDIR="../Applications/${APPLICATION_FULLNAME}"
if [ "$SKELETON" == "1" ]; then
    APPLICATION_SHORTNAME="${APPLICATION}${SKELETON_CHAR}"
else
    APPLICATION_SHORTNAME="${APPLICATION}${BENCHMARK_CHAR}${TRANSFER_CHAR}"
fi
APPDIR="../Applications/${APPLICATION_FULLNAME}/$APPLICATION_SHORTNAME"


# calculate how much time the script will take in minutes
TOTAL_RUNS=$((MAX_PAR * 7 * RUNS))
TOTAL_TIME=$((TOTAL_RUNS * (RUN_TIME + SLEEP_TIME)))

if [ "$SKELETON" == "1" ]; then
    TOTAL_RUNS=$((MAX_PAR * RUNS))
    TOTAL_TIME=$((TOTAL_RUNS * 4)) # 2**30 / (300 * 10**6) + 10 seconds (loading bitstream)
fi

# calculate hours, minutes and second to finish the script
TOTAL_TIME_MIN=$((TOTAL_TIME / 60))
TOTAL_TIME_SEC=$((TOTAL_TIME % 60))
TOTAL_TIME_HOUR=$((TOTAL_TIME_MIN / 60))
TOTAL_TIME_MIN=$((TOTAL_TIME_MIN % 60))

echo "Estimated total time for ${TOTAL_RUNS} runs: $TOTAL_TIME_HOUR hours $TOTAL_TIME_MIN minutes $TOTAL_TIME_SEC seconds"
END_TIME=$(date -d "$TOTAL_TIME seconds" +"%H:%M:%S")

if [ "$SKIP_CONFIRMATION" != "1" ]; then
    read -p "Press [y] to continue or [n] to abort: " -n 1 -r
    echo ""
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

compile_host

if [ "$SKELETON" == "1" ]; then
    run_benchmark_skeleton
else
    run_benchmark
fi

echo "Done!"