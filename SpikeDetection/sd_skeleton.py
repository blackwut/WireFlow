import os
from os import path
import sys
sys.path.insert(0, path.join(os.path.pardir))

from FSP import *


def round_up(n):
    if n == 0:
        return 1
    if n & (n - 1) == 0:
        return n
    while n & (n - 1) > 0:
        n &= (n - 1)
    return n << 1


generator_par = 1
avg_par = 1
spike_par = 1
collector_par = 1

generator_node = FOperator('generator',
                       generator_par,
                       FOperatorKind.GENERATOR,
                       FGatherMode.NONE,
                       FDispatchMode.KEYBY,
                       channel_depth=16)

avg_node = FOperator('average_calculator',
                 avg_par,
                 FOperatorKind.MAP,
                 FGatherMode.NON_BLOCKING,
                 FDispatchMode.RR_BLOCKING,
                 'tuple_t',
                 begin_function=True)

spike_node = FOperator('spike_detector',
                   spike_par,
                   FOperatorKind.FILTER,
                   FGatherMode.NON_BLOCKING,
                   FDispatchMode.RR_NON_BLOCKING)

collector_node = FOperator('collector',
                       collector_par,
                       FOperatorKind.COLLECTOR,
                       FGatherMode.NON_BLOCKING,
                       FDispatchMode.NONE)

generator_node.add_rng_state('int')
generator_node.add_rng_state('float')

win_dim = 16                              # window size
max_keys = 64                             # max num. of keys in total
avg_keys = round_up(max_keys // avg_par)  # max num. of keys per replica
threshold = 0.025                         # temperature threshold
constants = {'WIN_DIM': win_dim,
             'THRESHOLD': threshold,
             'MAX_KEYS': max_keys,
             'AVG_KEYS': avg_keys}

avg_node.add_private_buffer('int',
                            'sizes',
                            size=avg_keys,
                            attributes='__attribute__((register))')
avg_node.add_local_buffer('float',
                          'windows',
                          size=(avg_keys, win_dim))

pipe_folder = 'sd_skeleton{:02d}{:02d}{:02d}{:02d}'.format(generator_par,
                                                           avg_par,
                                                           spike_par,
                                                           collector_par)

pipe = FApplication(pipe_folder,
             'tuple_t',
             constants=constants,
             codebase='./codebase_skeleton')
pipe.add(generator_node)
pipe.add(avg_node)
pipe.add(spike_node)
pipe.add(collector_node)

pipe.finalize()
pipe.generate_device()
pipe.generate_host()
