import os
from os import path
import sys
sys.path.insert(0, path.join(os.path.pardir))

from FSP import *


def next_power_of_two(n):
    if n == 0:
        return 1
    if n & (n - 1) == 0:
        return n
    while n & (n - 1) > 0:
        n &= (n - 1)
    return n << 1


source_par = 1
predictor_par = 1
filter_par = 1
sink_par = 1

transfer_t = FTransferMode.SHARED   # COPY, HYBRID or SHARED
benchmark_t = 'throughput'          # latency or throughput
precision_t = 'float'               # float or double

transfer_char = ('s' if transfer_t == FTransferMode.SHARED else 'c')
benchmark_char = ('t' if benchmark_t == 'throughput' else 'l')
precision_char = ('f' if precision_t == 'float' else 'd')

num_states = 18
max_len = 16
win_dim = 5
threshold = 0.96
state_size = next_power_of_two(2**18 // predictor_par) # 2^18 = 262144 keys

constants = {'FLOAT_T': precision_t,
             'NUM_STATES': num_states,
             'MAX_LEN': max_len,
             'WIN_DIM': win_dim,
             'THRESHOLD': str(threshold) + ('f' if precision_t == 'float' else ''),
             'STATE_SIZE': state_size}

source_node = FOperator('source',
                    source_par,
                    FOperatorKind.SOURCE,
                    FGatherPolicy.NONE,
                    FDispatchPolicy.KB)

predictor_node = FOperator('predictor',
                       predictor_par,
                       FOperatorKind.MAP,
                       FGatherPolicy.LB,
                       FDispatchPolicy.RR,
                       o_datatype='tuple_t')

filter_node = FOperator('filter',
                    filter_par,
                    FOperatorKind.FILTER,
                    FGatherPolicy.LB,
                    FDispatchPolicy.RR)

sink_node = FOperator('sink',
                  sink_par,
                  FOperatorKind.SINK,
                  FGatherPolicy.LB,
                  FDispatchPolicy.NONE)

predictor_node.add_global_buffer('unsigned int',
                                 'num_states',
                                 1,
                                 FBufferAccess.READ_ALL,
                                 ptr=False,
                                 value=num_states)
predictor_node.add_global_buffer(precision_t,
                                 'trans_prob',
                                 (num_states, num_states),
                                 FBufferAccess.READ_ALL)
predictor_node.add_local_buffer('predictor_t',
                                'state',
                                state_size)

pipe_folder = 'fd{}{}{}{:d}{:d}{:d}{:d}'.format(transfer_char,
                                                benchmark_char,
                                                precision_char,
                                                source_par,
                                                predictor_par,
                                                filter_par,
                                                sink_par)

pipe = FApplication(pipe_folder,
             'input_t',
             constants=constants,
             transfer_mode=transfer_t,
             codebase='./codebase')
pipe.add_source(source_node)
pipe.add(predictor_node)
pipe.add(filter_node)
pipe.add_sink(sink_node)

pipe.finalize()
pipe.generate_device()
pipe.generate_host()
