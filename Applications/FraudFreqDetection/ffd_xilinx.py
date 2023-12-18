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

mr_node = FOperator('mr',
                    par,
                    FOperatorKind.MEMORY_READER,
                    FGatherPolicy.NONE,
                    FDispatchPolicy.KB)

predictor_node = FOperator('predictor',
                           par,
                           FOperatorKind.FLAT_MAP,
                           FGatherPolicy.LB,
                           FDispatchPolicy.KB,
                           o_datatype='tuple_t')

frequency_node = FOperator('frequency',
                            par,
                            FOperatorKind.MAP,
                            FGatherPolicy.LB,
                            FDispatchPolicy.RR,
                            o_datatype='result_t')

mw_node = FOperator('mw',
                    par,
                    FOperatorKind.MEMORY_WRITER,
                    FGatherPolicy.LB,
                    FDispatchPolicy.NONE)

pipe = FApplication('ffd' + get_transfer_char(transfer_t),
                    'input_t',
                    target = target_t,
                    constants = constants,
                    transfer_mode = transfer_t,
                    codebase = get_codebase(target_t))


pipe.add(mr_node)
pipe.add(predictor_node)
pipe.add(frequency_node)
pipe.add(mw_node)

pipe.generate_code(rewrite=True)
