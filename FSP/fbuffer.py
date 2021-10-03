import sys
from enum import Enum


# Global FBuffer access mode from device
class FBufferAccess(Enum):
    READ = 1        # Each replica has its own buffer in read mode
    WRITE = 2       # Each replica has its own buffer in write mode
    RW = 3          # Each replica has its own buffer in read-write mode
    READ_ALL = 4    # All replicas share the same buffer in read mode
    WRITE_ALL = 5   # All replicas share the same buffer in write mode
    RW_ALL = 6      # All replicas share the same buffer in read-write mode


# TODO: use a proper name instead of is_ptr_parameter()

class FBuffer:
    def __init__(self,
                 datatype: str,
                 name: str,
                 size=1,
                 value=None):
        assert datatype
        assert name

        self.datatype = datatype
        self.name = name

        # size handling
        if type(size) not in (int, tuple, list):
            sys.exit('"size" must be a int, tuple or list')

        self.size = [size] if type(size) is int else size
        for s in self.size:
            if s < 1:
                sys.exit('each element of "size" must be at least 1')

        # value handling
        self.value = value
        if self.value is not None:
            if type(value) in (int, float, str, bool, bytes):
                self.value = [value]
            elif type(value) in (tuple, list):
                self.value = value

        self.visibility = ''

    def value_to_str(self):
        # The following doesn't work
        # try:
        #     print('test' + str(val))
        #     return '{' + [value_to_str(x) for x in val] + '}'
        # except Exception as e:
        #     print('not test' + str(val))
        #     return '{' + ', '.join([str(x) for x in val]) + '}'

        # This one seems to work but is quite stupid
        val = self.value
        val = str(val).replace('(', '{').replace(')', '}')   # if there are tuples
        val = str(val).replace('[', '{').replace(']', '}')   # if there are lists
        return val

    def is_private(self):
        return False

    def is_local(self):
        return False

    def is_global(self):
        return False

    def declare(self):
        sys.exit('"declare()" function has to be implemented in a subclass')

    def parameter(self):
        sys.exit('"parameter()" function has to be implemented in a subclass')

    def use(self):
        sys.exit('"use()"" function has to be implemented in a subclass')


class FBufferPrivate(FBuffer):
    def __init__(self,
                 datatype: str,
                 name: str,
                 size=1,
                 value=None,
                 ptr: bool = False,    # set ptr = True if this buffer has to be passed by ref to a function even if size == 1
                 attributes=None):
        super().__init__(datatype, name, size, value)
        self.ptr = ptr
        self.attributes = attributes
        self.visibility = '__private'

    def is_ptr_parameter(self):
        return self.size[0] > 1 or self.ptr

    def is_private(self):
        return True

    def declare(self):
        d = ' '.join([self.visibility,
                      self.datatype])

        # TODO: Attributes must be a list or a dictionary to proper support attributes
        if self.attributes is not None:
            d += ' ' + self.attributes

        d += ' ' + self.name
        if self.size[0] > 1:
            d += '['
            d += ']['.join([str(s) for s in self.size])
            d += ']'

        if self.value is not None:
            d += ' = ' + self.value_to_str()
        return d

    def parameter(self):
        d = self.visibility + ' ' + self.datatype

        if self.size[0] > 1:
            d += ' ' + self.name
            d += '['
            d += ']['.join([str(s) for s in self.size])
            d += ']'
        elif self.ptr:
            d += ' * ' + self.name

        return d

    def use(self):
        if self.size[0] > 1 or not self.ptr:
            return self.name
        return '&' + self.name


class FBufferLocal(FBuffer):
    def __init__(self,
                 datatype: str,
                 name: str,
                 size=1,
                 value=None,
                 attributes=None):
        super().__init__(datatype, name, size)
        self.value = value
        self.attributes = attributes
        self.visibility = '__local'

    def is_ptr_parameter(self):
        return self.size[0] > 1

    def is_local(self):
        return True

    def declare(self):
        d = ' '.join([self.visibility,
                      self.datatype])

        if self.attributes is not None:
            d += ' ' + self.attributes

        d += ' ' + self.name
        if self.size[0] > 1:
            d += '['
            d += ']['.join([str(s) for s in self.size])
            d += ']'

        if self.value is not None:
            d += ' = ' + self.value_to_str()
        return d

    def parameter(self):
        d = self.visibility + ' ' + self.datatype + ' ' + self.name

        if self.size[0] > 1:
            d += '['
            d += ']['.join([str(s) for s in self.size])
            d += ']'
        return d

    def use(self):
        if self.is_ptr_parameter():
            return self.name
        return '&' + self.name


class FBufferGlobal(FBuffer):
    def __init__(self,
                 datatype: str,
                 name: str,
                 size=1,
                 access: FBufferAccess = FBufferAccess.READ_ALL,
                 ptr: bool = True,      # set ptr = False if you need to pass a single value to kernel
                 value=None):           # and fill its value
        super().__init__(datatype, name, size)
        self.access = access
        self.ptr = ptr
        self.value = value
        if not self.ptr and self.size[0] > 1:
            sys.exit(self.name + ' FBufferGlobal has to be of size = 1 if ptr is False')
        if not self.ptr and self.value is None:
            sys.exit(self.name + ' FBufferGlobal needs a value if ptr is False')
        self.visibility = '__global'

    def has_value(self):
        return (not self.ptr and self.value is not None)    # the second condition just to be sure

    def is_ptr_parameter(self):
        return self.size[0] > 1

    def is_global(self):
        return True

    def is_read_only(self):
        return self.access in (FBufferAccess.READ, FBufferAccess.READ_ALL)

    def is_write_only(self):
        return self.access in (FBufferAccess.WRITE, FBufferAccess.WRITE_ALL)

    def is_read_write(self):
        return self.access in (FBufferAccess.RW, FBufferAccess.RW_ALL)

    def is_access_single(self):
        return self.access in (FBufferAccess.READ, FBufferAccess.WRITE, FBufferAccess.RW)

    def is_access_all(self):
        return self.access in (FBufferAccess.READ_ALL, FBufferAccess.WRITE_ALL, FBufferAccess.RW_ALL)

    def declare(self):
        sys.exit('never call this function on FBufferGlobal')

    def parameter(self):
        const = ('const ' if self.is_read_only() else '')
        if self.is_ptr_parameter():
            return const + ' '.join([self.visibility,
                                     self.datatype,
                                     '*',
                                     'restrict',
                                     self.name])
        else:
            return const + ' '.join([self.datatype,
                                     self.name])

    def use(self):
        return self.name

#
# Jinja2 auxiliary functions
#

    def get_flags(self):
        if self.is_read_only():
            return 'CL_MEM_READ_ONLY'
        elif self.is_write_only():
            return 'CL_MEM_WRITE_ONLY'
        return 'CL_MEM_READ_WRITE'

    def get_queues_name(self, idx=None):
        return self.name + '_buffer_queues' + ('' if idx is None else '[' + str(idx) +']')

    def get_buffers_name(self, idx=None):
        return self.name + '_buffers' + ('' if idx is None else '[' + str(idx) +']')

    def get_declare_and_init(self):
        if self.has_value():
            return ' '.join([self.datatype,
                             self.name,
                             '=',
                             str(self.value)])
