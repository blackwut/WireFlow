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
avg_par = 1
spike_par = 1
sink_par = 1

source_node = FNode('source',
                    source_par,
                    FNodeKind.SOURCE,
                    FGatherMode.NONE,
                    FDispatchMode.KEYBY)

avg_node = FNode('average_calculator',
                 avg_par,
                 FNodeKind.MAP,
                 FGatherMode.NON_BLOCKING,
                 FDispatchMode.RR_BLOCKING,
                 begin_function=True)

spike_node = FNode('spike_detector',
                   spike_par,
                   FNodeKind.FILTER,
                   FGatherMode.NON_BLOCKING,
                   FDispatchMode.RR_NON_BLOCKING)

sink_node = FNode('sink',
                  sink_par,
                  FNodeKind.SINK,
                  FGatherMode.NON_BLOCKING,
                  FDispatchMode.NONE)

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
avg_node.add_local_buffer('float',
                          'windows',
                          size=(avg_keys, win_dim))

pipe_folder = 'sdb_float{:02d}{:02d}{:02d}{:02d}'.format(source_par,
                                                         avg_par,
                                                         spike_par,
                                                         sink_par)

pipe = FPipe(pipe_folder,
             'tuple_t',
             constants=constants,
             codebase='./codebase')
pipe.add_source(source_node)
pipe.add(avg_node)
pipe.add(spike_node)
pipe.add_sink(sink_node)

pipe.finalize()
pipe.generate_device()
pipe.generate_host()
