import argparse
import sys
sys.path.insert(0, '../..')
from FSPX import *


################################################################################
#
# Helper functions
#
################################################################################

def next_power_of_two(x):
    return 1 if x == 0 else 2**(x - 1).bit_length()

def get_codebase(target):
    basedir = './code'
    if target == FTarget.XILINX:
        return basedir + '/xilinx'
    elif target == FTarget.INTEL:
        return basedir + '/intel'
    else:
        raise ValueError('Unknown target')

def get_transfer_char(transfer):
    if transfer == FTransferMode.COPY:
        return 'c'
    elif transfer == FTransferMode.SHARED:
        return 's'
    elif transfer == FTransferMode.HOST:
        return 'h'
    else:
        raise ValueError('Unknown transfer mode')

def get_benchmark_char(benchmark):
    if benchmark == 'THROUGHPUT':
        return 't'
    elif benchmark == 'LATENCY':
        return 'l'
    else:
        raise ValueError('Unknown benchmark')


################################################################################
#
# Argument parsing
#
################################################################################

parser = argparse.ArgumentParser()
parser.add_argument('--par', type=int, default=1,
                    help='parallelism of operators')
parser.add_argument('--target', type=str, default='XILINX',
                    help='XILINX or INTEL')
parser.add_argument('--transfer', type=str, default='COPY',
                    help='COPY, SHARED (Intel), or HOST (Xilinx)')
parser.add_argument('--benchmark', type=str, default='THROUGHPUT',
                    help='THROUGHPUT or LATENCY')

if len(sys.argv) == 1:
    parser.print_help(sys.stderr)
    sys.exit(1)

args = parser.parse_args()

par = args.par
target = args.target
transfer = args.transfer

if target == 'XILINX':
    target_t = FTarget.XILINX
elif target == 'INTEL':
    target_t = FTarget.INTEL
else:
    raise ValueError('Unknown target')

if transfer == 'COPY':
    transfer_t = FTransferMode.COPY
elif transfer == 'SHARED':
    transfer_t = FTransferMode.SHARED
elif transfer == 'HOST':
    transfer_t = FTransferMode.HOST
else:
    raise ValueError('Unknown transfer mode')

if args.benchmark == 'THROUGHPUT':
    benchmark_t = 'THROUGHPUT'
elif args.benchmark == 'LATENCY':
    benchmark_t = 'LATENCY'
else:
    raise ValueError('Unknown benchmark')


################################################################################
#
# Application
#
################################################################################

win_dim = 16                                    # window size
max_keys = 64                                   # max n. of keys in total
avg_keys = next_power_of_two(max_keys // par)   # max n. of keys per replica
threshold = 0.25                                # property threshold
constants = {'MEASURE_LATENCY': 1 if benchmark_t == 'LATENCY' else 0,
             'WIN_DIM': win_dim,
             'THRESHOLD': str(threshold) + 'f',
             'MAX_KEYS': max_keys,
             'AVG_KEYS': avg_keys}

mr_node = FOperator('mr',
                    par,
                    FOperatorKind.MEMORY_READER,
                    FGatherPolicy.NONE,
                    FDispatchPolicy.KB)

avg_node = FOperator('average_calculator',
                     par,
                     FOperatorKind.MAP,
                     FGatherPolicy.LB,
                     FDispatchPolicy.RR,
                     o_datatype='tuple_t')

spike_node = FOperator('spike_detector',
                       par,
                       FOperatorKind.FILTER,
                       FGatherPolicy.LB,
                       FDispatchPolicy.LB)

mw_node = FOperator('mw',
                    par,
                    FOperatorKind.MEMORY_WRITER,
                    FGatherPolicy.LB,
                    FDispatchPolicy.NONE)

pipe = FApplication('sd' + get_benchmark_char(benchmark_t) + get_transfer_char(transfer_t),
                    'input_t',
                    target = target_t,
                    constants = constants,
                    transfer_mode = transfer_t,
                    codebase = get_codebase(target_t))
pipe.add(mr_node)
pipe.add(avg_node)
pipe.add(spike_node)
pipe.add(mw_node)

pipe.generate_code(rewrite=True)
