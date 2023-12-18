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

def get_codebase():
    return './code/xilinx_skeleton'

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
    if benchmark == 'throughput':
        return 't'
    elif benchmark == 'latency':
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

args = parser.parse_args()
par = args.par
target_t = FTarget.XILINX

################################################################################
#
# Application
#
################################################################################

win_dim = 16                                    # window size
max_keys = 64                                   # max n. of keys in total
avg_keys = next_power_of_two(max_keys // par)   # max n. of keys per replica
threshold = 0.25                                # property threshold
constants = {'WIN_DIM': win_dim,
             'THRESHOLD': str(threshold) + 'f',
             'MAX_KEYS': max_keys,
             'AVG_KEYS': avg_keys}

mr_node = FOperator('generator',
                    par,
                    FOperatorKind.GENERATOR,
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

mw_node = FOperator('drainer',
                    par,
                    FOperatorKind.DRAINER,
                    FGatherPolicy.LB,
                    FDispatchPolicy.NONE)

pipe = FApplication('sds',
                    'input_t',
                    target = target_t,
                    constants = constants,
                    codebase = get_codebase())

pipe.add(mr_node)
pipe.add(avg_node)
pipe.add(spike_node)
pipe.add(mw_node)

pipe.generate_code(rewrite=True)
