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
num_states = 18
max_len = 16
win_dim = 5
threshold = 0.96
state_size = next_power_of_two(2**18 // par) # 2^18 = 262144 keys

constants = {'NUM_STATES': num_states,
             'MAX_LEN': max_len,
             'WIN_DIM': win_dim,
             'THRESHOLD': str(threshold) + 'f',
             'STATE_SIZE': state_size}

mr_node = FOperator('generator',
                    par,
                    FOperatorKind.GENERATOR,
                    FGatherPolicy.NONE,
                    FDispatchPolicy.KB)

predictor_node = FOperator('predictor',
                           par,
                           FOperatorKind.FLAT_MAP,
                           FGatherPolicy.LB,
                           FDispatchPolicy.RR,
                           o_datatype='tuple_t')

mw_node = FOperator('drainer',
                    par,
                    FOperatorKind.DRAINER,
                    FGatherPolicy.LB,
                    FDispatchPolicy.NONE)

pipe = FApplication('fds',
                    'input_t',
                    target = target_t,
                    constants = constants,
                    codebase = get_codebase())


pipe.add(mr_node)
pipe.add(predictor_node)
pipe.add(mw_node)

pipe.generate_code(rewrite=True)
