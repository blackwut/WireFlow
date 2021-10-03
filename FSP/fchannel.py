from .fnode import FNode


class FChannel:
    def __init__(self,
                 i_node: FNode,
                 o_node: FNode,
                 depth: int = 0):
        assert i_node
        assert o_node
        assert depth >= 0

        self.i_node = i_node
        self.o_node = o_node

        self.datatype = i_node.o_datatype
        self.name = i_node.name + '_' + o_node.name
        self.name_ch = self.name + '_ch'
        self.depth = depth
        self.i_degree = i_node.par
        self.o_degree = o_node.par

        self.tupletype = self.name + '_t'

#
# Jinja2 auxiliary functions
#

    def declare(self):
        d = ' '.join(['channel',
                      self.tupletype,
                      self.name_ch])
        if self.i_degree > 1:
            d += '[' + str(self.i_node.par) + ']'
        if self.o_degree > 1:
            d += '[' + str(self.o_node.par) + ']'

        if self.depth > 0:
            d += ' __attribute__((depth(' + str(self.depth) + ')))'
        return d

    def use(self, i, j):
        u = self.name_ch
        if i is not None and self.i_node.par > 1:
            u += '[' + str(i) + ']'
        if j is not None and self.o_node.par > 1:
            u += '[' + str(j) + ']'
        return u

    def read(self, i, j):
        return 'read_channel_intel(' + self.use(i, j) + ')'

    def read_nb(self, i, j, valid):
        return 'read_channel_nb_intel(' + self.use(i, j) + ', &' + valid + ')'

    def write(self, i, j, value):
        return 'write_channel_intel(' + self.use(i, j) + ', ' + value + ')'

    def write_nb(self, i, j, value):
        return 'write_channel_nb_intel(' + self.use(i, j) + ', ' + value + ')'
