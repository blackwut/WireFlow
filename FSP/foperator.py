import sys
import random
from enum import Enum

from .fdispatch import FDispatchMode
from .fgather import FGatherMode
from .fbuffer import FBufferPrivate, FBufferLocal, FBufferGlobal, FBufferAccess


class FOperatorKind(Enum):
    NONE = 1
    SOURCE = 2
    FILTER = 3
    MAP = 4
    FLAT_MAP = 5
    SINK = 6
    GENERATOR = 7
    COLLECTOR = 8


class FOperator:
    def __init__(self,
                 name: str,
                 par: int,
                 kind: FOperatorKind,
                 gather_mode: FGatherMode,
                 dispatch_mode: FDispatchMode,
                 o_datatype: str = None,
                 channel_depth: int = 0,
                 begin_function: bool = False,
                 compute_function: bool = False,
                 end_function: bool = False):
        assert name
        assert par > 0
        assert isinstance(kind, FOperatorKind)
        assert isinstance(gather_mode, FGatherMode)
        assert isinstance(dispatch_mode, FDispatchMode)
        # assert o_datatype or kind is FNodeKind.COLLECTOR
        assert channel_depth >= 0

        self.id = -1
        self.name = name
        self.par = par
        self.kind = kind
        self.gather_mode = gather_mode
        self.dispatch_mode = dispatch_mode
        self.i_datatype = ''
        self.o_datatype = o_datatype
        self.channel_depth = channel_depth
        self.begin_function = begin_function
        self.compute_function = (kind not in (FOperatorKind.SOURCE, FOperatorKind.SINK, FOperatorKind.COLLECTOR)) or compute_function
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

    def is_gather_b(self):
        return self.gather_mode.is_b()

    def is_gather_nb(self):
        return self.gather_mode.is_nb()

    def is_dispatch_RR(self):
        return self.dispatch_mode.is_RR()

    def is_dispatch_KEYBY(self):
        return self.dispatch_mode.is_KEYBY()

    def is_dispatch_BROADCAST(self):
        return self.dispatch_mode.is_BROADCAST()

# FNodeKind
    def is_source(self):
        return self.kind == FOperatorKind.SOURCE

    def is_filter(self):
        return self.kind == FOperatorKind.FILTER

    def is_map(self):
        return self.kind == FOperatorKind.MAP

    def is_flat_map(self):
        return self.kind == FOperatorKind.FLAT_MAP

    def is_sink(self):
        return self.kind == FOperatorKind.SINK

    def is_generator(self):
        return self.kind == FOperatorKind.GENERATOR

    def is_collector(self):
        return self.kind == FOperatorKind.COLLECTOR

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
