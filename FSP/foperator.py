import sys
import random
from enum import Enum

from .fdispatch import FDispatchPolicy
from .fgather import FGatherPolicy
from .fbuffer import FBufferPrivate, FBufferLocal, FBufferGlobal, FBufferAccess


class FOperatorKind(Enum):
    NONE = 1
    MEMORY_READER = 2
    FILTER = 3
    MAP = 4
    FLAT_MAP = 5
    MEMORY_WRITER = 6
    GENERATOR = 7
    DRAINER = 8


class FOperator:
    def __init__(self,
                 name: str,
                 par: int,
                 kind: FOperatorKind,
                 gather_policy: FGatherPolicy,
                 dispatch_policy: FDispatchPolicy,
                 o_datatype: str = None,
                 channel_depth: int = 0,
                 begin_function: bool = False,
                 compute_function: bool = False,
                 end_function: bool = False):
        assert name
        assert par > 0
        assert isinstance(kind, FOperatorKind)
        assert isinstance(gather_policy, FGatherPolicy)
        assert isinstance(dispatch_policy, FDispatchPolicy)
        # assert o_datatype or kind is FNodeKind.DRAINER
        assert channel_depth >= 0

        if kind is FOperatorKind.MEMORY_READER:
            assert gather_policy is FGatherPolicy.NONE
        if kind is FOperatorKind.MEMORY_WRITER:
            assert dispatch_policy is FDispatchPolicy.NONE

        self.id = -1
        self.name = name
        self.par = par
        self.kind = kind
        self.gather_policy = gather_policy
        self.dispatch_policy = dispatch_policy
        self.i_datatype = ''
        self.o_datatype = o_datatype
        self.channel_depth = channel_depth
        self.begin_function = begin_function
        self.compute_function = (kind not in (FOperatorKind.MEMORY_READER, FOperatorKind.MEMORY_WRITER, FOperatorKind.DRAINER)) or compute_function
        self.end_function = end_function

        self.i_channel = None
        self.o_channel = None
        self.buffers = []

    def check_buffer_duplicate(self, name):
        for b in self.buffers:
            if name == b.name:
                return True
        return False

    def add_private_buffer(self,
                           datatype: str,
                           name: str,
                           size: int = 1,
                           value=None,
                           ptr: bool = False,
                           attributes=None):
        if self.check_buffer_duplicate(name):
            sys.exit('Buffer "' + name + '" in node "' + self.name + '" is already present!')
        self.buffers.append(FBufferPrivate(datatype, name, size, value, ptr, attributes))

    def add_local_buffer(self,
                         datatype: str,
                         name: str,
                         size: int = 1,
                         value=None,
                         attributes=None):
        if self.check_buffer_duplicate(name):
            sys.exit('Buffer "' + name + '" in node "' + self.name + '" is already present!')
        self.buffers.append(FBufferLocal(datatype, name, size, value, attributes))

    def add_global_buffer(self,
                          datatype: str,
                          name: str,
                          size: int = 1,
                          access: FBufferAccess = FBufferAccess.READ_ALL,
                          ptr: bool = True,
                          value=None):
        if self.check_buffer_duplicate(name):
            sys.exit('Buffer "' + name + '" in node "' + self.name + '" is already present!')
        self.buffers.append(FBufferGlobal(datatype, name, size, access, ptr, value))

    def get_buffers(self):
        return self.buffers

    def get_private_buffers(self):
        return [b for b in self.buffers if type(b) is FBufferPrivate]

    def get_local_buffers(self):
        return [b for b in self.buffers if type(b) is FBufferLocal]

    def get_global_value_buffers(self):
        return [b for b in self.buffers if type(b) is FBufferGlobal and b.has_value()]

    def get_global_no_value_buffers(self):
        return [b for b in self.buffers if type(b) is FBufferGlobal and not b.has_value()]

    def get_global_buffers(self):
        # return [b for b in self.buffers if type(b) is FBufferGlobal]
        wo_value = self.get_global_no_value_buffers()
        w_value = self.get_global_value_buffers()
        return wo_value + w_value

#
# Jinja2 auxiliary functions
#

    def kernel_name(self, idx):
        return self.name + '_' + str(idx)

    def has_begin_function(self):
        return self.begin_function

    def has_compute_function(self):
        return self.compute_function

    def has_end_function(self):
        return self.end_function

    def has_functions(self):
        """
        returns true if any of begin, compute or end functions has to be
        implemented by the user
        """
        return (self.has_begin_function()
                or self.has_compute_function()
                or self.has_end_function())

    def begin_function_name(self):
        return self.name + '_begin'

    def function_name(self):
        return self.name + '_function'

    def end_function_name(self):
        return self.name + '_end'

    def call_begin_function(self, param=None):
        return (self.begin_function_name()
                + '('
                + (param if param is not None else '')
                + (', ' if param is not None and len(self.get_buffers()) > 0 else '')
                + self.use_buffers() + ')')

    def call_function(self, param=None):
        if self.has_compute_function():
            return (self.function_name()
                    + '('
                    + (param if param is not None else '')
                    + (', ' if param is not None and len(self.get_buffers()) > 0 else '')
                    + self.use_buffers() + ')')
        else:
            return param

    def call_end_function(self, param=None):
        return (self.end_function_name()
                + '('
                + (param if param is not None else '')
                + (', ' if param is not None and len(self.get_buffers()) > 0 else '')
                + self.use_buffers() + ')')

    def is_gather_RR(self):
        return self.gather_policy.is_RR()

    def is_gather_LB(self):
        return self.gather_policy.is_LB()

    def is_gather_KB(self):
        return self.gather_policy.is_KB()

    def is_dispatch_RR(self):
        return self.dispatch_policy.is_RR()

    def is_dispatch_LB(self):
        return self.dispatch_policy.is_LB()

    def is_dispatch_KB(self):
        return self.dispatch_policy.is_KB()

    def is_dispatch_BR(self):
        return self.dispatch_policy.is_BR()

# FNodeKind
    def is_memory_reader(self):
        return self.kind == FOperatorKind.MEMORY_READER

    def is_filter(self):
        return self.kind == FOperatorKind.FILTER

    def is_map(self):
        return self.kind == FOperatorKind.MAP

    def is_flat_map(self):
        return self.kind == FOperatorKind.FLAT_MAP

    def is_memory_writer(self):
        return self.kind == FOperatorKind.MEMORY_WRITER

    def is_generator(self):
        return self.kind == FOperatorKind.GENERATOR

    def is_drainer(self):
        return self.kind == FOperatorKind.DRAINER

# Channels
    def read(self, i, j):
        return self.i_channel.read(i, j)

    def read_nb(self, i, j, valid):
        return self.i_channel.read_nb(i, j, valid)

    def write(self, i, j, value):
        return self.o_channel.write(i, j, value)

    def write_nb(self, i, j, value):
        return self.o_channel.write_nb(i, j, value)

# Buffers
    def declare_buffers(self):
        return ''.join([b.declare() + ';\n' for b in self.get_buffers()])

    def declare_private_buffers(self):
        return ''.join([b.declare() + ';\n' for b in self.get_private_buffers()])

    def declare_local_buffers(self):
        return ''.join([b.declare() + ';\n' for b in self.get_local_buffers()])

    def declare_global_buffers(self):
        return ''.join([b.declare() + ';\n' for b in self.get_global_buffers()])

    def parameter_buffers(self):
        return ', '.join(b.parameter() for b in self.get_buffers())

    def parameter_private_buffers(self):
        return ', '.join([b.parameter() for b in self.get_private_buffers()])

    def parameter_local_buffers(self):
        return ', '.join([b.parameter() for b in self.get_local_buffers()])

    def parameter_global_buffers(self):
        return ', '.join([b.parameter() for b in self.get_global_buffers()])

    def parameter_buffers_list(self):
        return [b.parameter() for b in self.get_buffers()]

    def parameter_private_buffers_list(self):
        return [b.parameter() for b in self.get_private_buffers()]

    def parameter_local_buffers_list(self):
        return [b.parameter() for b in self.get_local_buffers()]

    def parameter_global_buffers_list(self):
        return [b.parameter() for b in self.get_global_buffers()]

    def use_buffers(self):
        return ', '.join([b.use() for b in self.get_buffers()])

    def use_private_buffers(self):
        return ', '.join([b.use() for b in self.get_private_buffers()])

    def use_local_buffers(self):
        return ', '.join([b.use() for b in self.get_local_buffers()])

    def use_global_buffers(self):
        return ', '.join([b.use() for b in self.get_global_buffers()])

# Tuples
    def i_tupletype(self):
        return self.i_channel.tupletype

    def o_tupletype(self):
        return self.o_channel.tupletype

    def declare_i_tuple(self, name):
        return self.i_tupletype() + ' ' + name

    def declare_o_tuple(self, name):
        return self.o_tupletype() + ' ' + name

    def create_i_tuple(self, name, parameter):
        tupletype = self.i_tupletype()
        return tupletype + ' ' + name + ' = create_' + tupletype + '(' + parameter + ')'

    def create_o_tuple(self, name, parameter):
        tupletype = self.o_tupletype()
        return 'const ' + tupletype + ' ' + name + ' = create_' + tupletype + '(' + parameter + ')'

# Generator Node
    def add_rng_state(self, state_name):
        assert self.is_generator()
        rng = 0
        while rng == 0:
            rng = random.randrange(2**32 - 1)
        self.add_private_buffer('rng_state_t', 'rng_' + state_name, ptr=True, value=rng)

# XILINX
    def get_par_macro(self):
        return self.name.upper() + '_PAR'

    def get_type_name(self):
        if self.kind == FOperatorKind.MEMORY_READER:
            return 'MR'
        elif self.kind == FOperatorKind.FILTER:
            return 'Filter'
        elif self.kind == FOperatorKind.MAP:
            return 'Map'
        elif self.kind == FOperatorKind.FLAT_MAP:
            return 'FlatMap'
        elif self.kind == FOperatorKind.MEMORY_WRITER:
            return 'MW'
        elif self.kind == FOperatorKind.GENERATOR:
            return 'Generator'
        elif self.kind == FOperatorKind.DRAINER:
            return 'Collector'
        else:
            sys.exit('Unknown node kind!')

    def get_keyby_lambda_name(self):
        return self.name + '_keyby'

    def get_gather_name(self):
        if self.is_gather_RR():
            return 'RR'
        elif self.is_gather_LB():
            return 'LB'
        elif self.is_gather_KB():
            return 'KB'
        else:
            sys.exit('Unknown gather policy!')

    def get_dispatch_name(self):
        if self.is_dispatch_RR():
            return 'RR'
        elif self.is_dispatch_LB():
            return 'LB'
        elif self.is_dispatch_KB():
            return 'KB'
        elif self.is_dispatch_BR():
            return 'BR'
        else:
            sys.exit('Unknown dispatch policy!')
