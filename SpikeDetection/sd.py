import sys
sys.path.insert(0, '..')
from FSPX import *


def next_power_of_two(x):
    return 1 if x == 0 else 2**(x - 1).bit_length()


par = 1                             # n. of replicas
target_t = FTarget.XILINX           # XILINX or INTEL
transfer_t = FTransferMode.HOST     # COPY, SHARED (Intel), or HOST (Xilinx)
benchmark_t = 'throughput'          # throughput or latency

win_dim = 16                                    # window size
max_keys = 64                                   # max n. of keys in total
avg_keys = next_power_of_two(max_keys // par)   # max n. of keys per replica
threshold = 0.025                               # property threshold
constants = {'MEASURE_LATENCY': 1 if benchmark_t == 'latency' else 0,
             'WIN_DIM': win_dim,
             'THRESHOLD': threshold,
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
                     begin_function=True,
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

avg_node.add_private_buffer('int',
                            'sizes',
                            size=avg_keys,
                            attributes='__attribute__((register))')
avg_node.add_local_buffer('float',
                          'windows',
                          size=(avg_keys, win_dim))

codebase_t = './code/' + ('xilinx' if target_t == FTarget.XILINX else 'intel')
transfer_char = ('s' if transfer_t == FTransferMode.SHARED else 'c' if transfer_t == FTransferMode.COPY else 'h')
benchmark_char = ('t' if benchmark_t == 'throughput' else 'l')

pipe = FApplication('sd' + benchmark_char + transfer_char,
                    'input_t',
                    target = target_t,
                    constants=constants,
                    transfer_mode=transfer_t,
                    codebase=codebase_t)
pipe.add(mr_node)
pipe.add(avg_node)
pipe.add(spike_node)
pipe.add(mw_node)

pipe.generate_code(rewrite=True)
