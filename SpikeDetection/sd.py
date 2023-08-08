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


mr_par = 1
avg_par = 1
spike_par = 1
mw_par = 1

transfer_t = FTransferMode.COPY     # COPY, HYBRID or SHARED
benchmark_t = 'throughput'          # latency or throughput
precision_t = 'float'               # float or double
in_datatype_t = ('input_t' if benchmark_t == 'throughput' else 'tuple_t')

transfer_char = ('s' if transfer_t == FTransferMode.SHARED else 'c')
benchmark_char = ('t' if benchmark_t == 'throughput' else 'l')
precision_char = ('f' if precision_t == 'float' else 'd')

mr_node = FOperator('mr',
                    mr_par,
                    FOperatorKind.MEMORY_READER,
                    FGatherPolicy.NONE,
                    FDispatchPolicy.KB)

avg_node = FOperator('average_calculator',
                     avg_par,
                     FOperatorKind.MAP,
                     FGatherPolicy.LB,
                     FDispatchPolicy.RR,
                     begin_function=True,
                     o_datatype='tuple_t')

spike_node = FOperator('spike_detector',
                       spike_par,
                       FOperatorKind.FILTER,
                       FGatherPolicy.LB,
                       FDispatchPolicy.LB)

mw_node = FOperator('mw',
                    mw_par,
                    FOperatorKind.MEMORY_WRITER,
                    FGatherPolicy.LB,
                    FDispatchPolicy.NONE)

win_dim = 16                                       # window size
max_keys = 64                                      # max n. of keys in total
avg_keys = next_power_of_two(max_keys // avg_par)  # max n. of keys per replica
threshold = 0.025                                  # property threshold
constants = {'WIN_DIM': win_dim,
             'THRESHOLD': threshold,
             'MAX_KEYS': max_keys,
             'AVG_KEYS': avg_keys}

avg_node.add_private_buffer('int',
                            'sizes',
                            size=avg_keys,
                            attributes='__attribute__((register))')
avg_node.add_local_buffer(precision_t,
                          'windows',
                          size=(avg_keys, win_dim))

pipe_folder = 'sd{}{}{}{:d}{:d}{:d}{:d}'.format(transfer_char,
                                                benchmark_char,
                                                precision_char,
                                                mr_par,
                                                avg_par,
                                                spike_par,
                                                mw_par)

pipe = FApplication(pipe_folder,
                    in_datatype_t,
                    target = FTarget.XILINX,
                    constants=constants,
                    transfer_mode=transfer_t,
                    codebase="./codebase")
pipe.add(mr_node)
pipe.add(avg_node)
pipe.add(spike_node)
pipe.add(mw_node)

pipe.generate_device(rewrite=True)
pipe.generate_host()
