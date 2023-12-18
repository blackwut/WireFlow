#!/bin/bash

function enter_venv() {
    local VENV_DIR="../env"

    if [ ! -d "$VENV_DIR" ]; then
        echo "Creating virtual environment..."
        python3 -m venv $VENV_DIR
        source $VENV_DIR/bin/activate
        pip install -r requirements.txt
    else
        if [[ -z "${VIRTUAL_ENV}" ]]; then
            echo "Activating virtual environment..."
            source $VENV_DIR/bin/activate
        fi
    fi
}

function exit_venv() {
    deactivate
}

function generate_application() {
    local BASEDIR="../Applications/${APPLICATION_NAME}"

    local BENCHMARK_FLAG=""
    if [ "$APPLICATION" == "sd" ]; then
        BENCHMARK_FLAG="--benchmark ${BENCHMARK}"
    fi

    local PYTHON_SCRIPT="${APPLICATION}_xilinx"
    local PYTHON_ARGS="--par 1"
    if [ "$SKELETON" == "1" ]; then
        PYTHON_SCRIPT="${PYTHON_SCRIPT}_skeleton"
    else
        PYTHON_ARGS="${PYTHON_ARGS} --target XILINX --transfer ${TRANSFER} ${BENCHMARK_FLAG}"
    fi

    cd $BASEDIR
    echo "python ${PYTHON_SCRIPT}.py ${PYTHON_ARGS}"

    if [ "$DRY_RUN" == "1" ]; then
        cd -
        return
    fi

    python ${PYTHON_SCRIPT}.py ${PYTHON_ARGS}
    cd -
}

function compile_application() {
    local BASEDIR="../Applications/${APPLICATION_NAME}"

    if [ "$SKELETON" == "1" ]; then
        BASEDIR="${BASEDIR}/${APPLICATION}${SKELETON_CHAR}"
    else
        BASEDIR="${BASEDIR}/${APPLICATION}${BENCHMARK_CHAR}${TRANSFER_CHAR}"
    fi

    echo "cd $BASEDIR"
    if [ "$DRY_RUN" != "1" ]; then
        cd $BASEDIR
    fi

    if [ "$TRANSFER" == "HOST" ]; then
        HOSTMEM=1
    else
        HOSTMEM=0
    fi

    if [ "$APPLICATION_NAME" == "SpikeDetection" ]; then
        echo "make bitstream TARGET=hw HOSTMEM=$HOSTMEM MR_PAR=$PAR GENERATOR_PAR=$PAR AVERAGE_CALCULATOR_PAR=$PAR SPIKE_DETECTOR_PAR=$PAR MW_PAR=$PAR DRAINER_PAR=$PAR"
        if [ "$DRY_RUN" == "1" ]; then
            cd -
            return
        fi
        make bitstream TARGET=hw HOSTMEM=$HOSTMEM MR_PAR=$PAR GENERATOR_PAR=$PAR AVERAGE_CALCULATOR_PAR=$PAR SPIKE_DETECTOR_PAR=$PAR MW_PAR=$PAR DRAINER_PAR=$PAR
        cd -
        return
    elif [ "$APPLICATION_NAME" == "FraudDetection" ]; then
        echo "make bitstream TARGET=hw HOSTMEM=$HOSTMEM MR_PAR=$PAR GENERATOR_PAR=$PAR PREDICTOR_PAR=$PAR MW_PAR=$PAR DRAINER_PAR=$PAR"
        if [ "$DRY_RUN" == "1" ]; then
            cd -
            return
        fi
        make bitstream TARGET=hw HOSTMEM=$HOSTMEM MR_PAR=$PAR GENERATOR_PAR=$PAR PREDICTOR_PAR=$PAR MW_PAR=$PAR DRAINER_PAR=$PAR
        cd -
        return
    else
        echo "make bitstream TARGET=hw HOSTMEM=$HOSTMEM MR_PAR=$PAR GENERATOR_PAR=$PAR PREDICTOR_PAR=$PAR FREQUENCY_PAR=$PAR MW_PAR=$PAR DRAINER_PAR=$PAR"
        if [ "$DRY_RUN" == "1" ]; then
            cd -
            return
        fi
        make bitstream TARGET=hw HOSTMEM=$HOSTMEM MR_PAR=$PAR GENERATOR_PAR=$PAR PREDICTOR_PAR=$PAR FREQUENCY_PAR=$PAR MW_PAR=$PAR DRAINER_PAR=$PAR
        cd -
        return
    fi
    cd -
}

while getopts 'a:t:b:snh' flag; do
  case "${flag}" in
    a) APPLICATION="${OPTARG}" ;;
    t) TRANSFER="${OPTARG}" ;;
    b) BENCHMARK="${OPTARG}" ;;
    s) SKELETON=1 ;;
    n) DRY_RUN=1 ;;
    h) echo "Usage: $0 [-a APPLICATION] [-t TRANSFER] [-b BENCHMARK] [-s] [-n] [-h]"
       echo "  -a APPLICATION: 'sd' for SpikeDetection, 'fd' for FraudDetection, or 'ffd' for FraudFreqDetection"
       echo "  -t TRANSFER: 'COPY' or 'HOST'"
       echo "  -b BENCHMARK: 'latency' or 'throughput'"
       echo "  -s: skeleton version"
       echo "  -n: dry run, do not compile"
       echo "  -h: show this help"
       exit 0 ;;
    *) echo "Unexpected option ${flag}" >&2; exit 1 ;;
  esac
done

# check if APPLICATION is "sd", "fd" or "ffd'
if [ "$APPLICATION" != "sd" ] && [ "$APPLICATION" != "fd" ] && [ "$APPLICATION" != "ffd" ]; then
    echo "Application must be 'sd', 'fd' or 'ffd'!"
    exit 1
fi

# set APPLICATION_NAME ("SpikeDetection", "FraudDetection", or "FraudFreqDetection")
if [ "$APPLICATION" == "sd" ]; then
    APPLICATION_NAME="SpikeDetection"
elif [ "$APPLICATION" == "fd" ]; then
    APPLICATION_NAME="FraudDetection"
else
    APPLICATION_NAME="FraudFreqDetection"
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

enter_venv
generate_application
for PAR in $(seq 2 $MAX_PAR); do
    echo "Compiling ${APPLICATION_NAME} with PAR=${PAR}"
    compile_application
done
exit_venv

echo "Done!"