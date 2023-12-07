import sys
sys.path.insert(0, '..')
from FSPX import *


def next_power_of_two(x):
    return 1 if x == 0 else 2**(x - 1).bit_length()

par = 1                             # n. of replicas
target_t = FTarget.XILINX           # XILINX or INTEL
transfer_t = FTransferMode.HOST     # COPY, SHARED (Intel), or HOST (Xilinx)

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
                           FDispatchPolicy.RR,
                           o_datatype='tuple_t')

mw_node = FOperator('mw',
                    par,
                    FOperatorKind.MEMORY_WRITER,
                    FGatherPolicy.LB,
                    FDispatchPolicy.NONE)

codebase_t = './code/' + ('xilinx' if target_t == FTarget.XILINX else 'intel')
transfer_char = ('s' if transfer_t == FTransferMode.SHARED else 'c' if transfer_t == FTransferMode.COPY else 'h')

pipe = FApplication('fd' + transfer_char,
                    'input_t',
                    target = target_t,
                    constants=constants,
                    transfer_mode=transfer_t,
                    codebase=codebase_t)


pipe.add(mr_node)
pipe.add(predictor_node)
pipe.add(mw_node)

pipe.generate_code(rewrite=True)
