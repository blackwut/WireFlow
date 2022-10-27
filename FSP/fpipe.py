import sys
from os import path
from enum import Enum
from shutil import copyfile
from .futils import *
from .fnode import *
from .fchannel import *

# TODO: add check on all types (including FBuffer types) before create device/host files

class FTransferMode(Enum):
    COPY = 1
    SHARED = 2


class FPipe:

    def __init__(self,
                 dest_dir: str,
                 datatype: str,
                 transfer_mode: FTransferMode = FTransferMode.COPY,
                 codebase: str = None,
                 constants: dict = {}):
        assert dest_dir

        self.dest_dir = dest_dir
        self.datatype = datatype
        self.transfer_mode = transfer_mode
        self.codebase = codebase
        self.constants = constants

        self.source = None
        self.internal_nodes = []
        self.sink = None

    def prepare_folders(self):
        """
        app
        ├── common
        ├── device
        │   └── includes
        │   └── nodes
        ├── host
        ├── includes
        └── sources
        """
        self.base_dir = self.dest_dir
        self.common_dir = path.join(self.dest_dir, 'common')
        self.includes_dir = path.join(self.dest_dir, 'includes')
        self.host_dir = path.join(self.dest_dir, 'host')
        self.device_dir = path.join(self.dest_dir, 'device')
        self.dev_inc_dir = path.join(self.device_dir, 'includes')
        self.nodes_dir = path.join(self.device_dir, 'nodes')

        for folder in (self.base_dir, self.common_dir, self.includes_dir,
                       self.host_dir, self.device_dir, self.dev_inc_dir,
                       self.nodes_dir):
            if not path.isdir(folder):
                os.mkdir(folder)

    def get_nodes(self):
        nodes = []
        if self.source:
            nodes.append(self.source)
        nodes.extend(self.internal_nodes)
        if self.sink:
            nodes.append(self.sink)
        return nodes

    def get_par_constants(self):
        cs = {}
        nodes = self.get_nodes()
        for n in nodes:
            key = '__' + n.name.upper() + '_PAR'
            cs[key] = n.par
        return cs

#
# User's functions
#
    def add_source(self,
                   source: FNode):
        assert source
        assert not self.source

        if source.kind == FNodeKind.SOURCE:
            self.source = source
        else:
            sys.exit("Supplied node is not of type SOURCE")

    def add_sink(self,
                 sink: FNode):
        assert sink
        assert not self.sink

        if sink.kind == FNodeKind.SINK:
            self.sink = sink
        else:
            sys.exit("Supplied node is not of type SINK")

    def add(self,
            node: FNode):
        assert node

        if node.kind not in (FNodeKind.SOURCE, FNodeKind.SINK):
            self.internal_nodes.append(node)
        else:
            sys.exit("Supplied node is not of type MAP, FILTER, FLAT_MAP, GENERATOR, or COLLECTOR")

    def finalize(self):
        nodes = self.get_nodes()

        # Checks duplicate names
        names = set()
        for n in nodes:
            if n.name in names:
                sys.exit("Node's name '" + n.name + "' already taken!")
            else:
                names.add(n.name)

        # Updates input and output degree
        for prv, cur, nxt in previous_current_next(nodes):
            cur.i_degree = prv.par if prv is not None else 0
            cur.o_degree = nxt.par if nxt is not None else 0

        # Updates input and output datatype
        for prv, cur, nxt in previous_current_next(nodes):
            cur.i_datatype = prv.o_datatype if prv is not None else self.datatype
            if cur.o_datatype is None:
                cur.o_datatype = cur.i_datatype

        # Creates channels
        self.channels = []
        for prv, cur, nxt in previous_current_next(nodes):
            if prv:
                c = FChannel(prv, cur, prv.channel_depth)
                prv.o_channel = c
                cur.i_channel = c
                self.channels.append(c)

        # Creates folders
        self.prepare_folders()

    def generate_fsp(self):
        filename = 'fsp.cl'
        filepath = path.join(self.dev_inc_dir, filename)

        template = read_template_file(self.dest_dir, 'fsp.cl')
        file = open(filepath, mode='w+')
        result = template.render(sink=self.sink)
        file.write(result)
        file.close()

    def generate_fsp_tuples(self):
        filename = 'fsp_tuples.cl'
        filepath = path.join(self.dev_inc_dir, filename)

        template = read_template_file(self.dest_dir, 'fsp_tuples.cl')
        file = open(filepath, mode='w+')
        result = template.render(channels=self.channels)
        file.write(result)
        file.close()

    def generate_tuples(self,
                        rewrite=False):
        filename = 'tuples.h'
        filepath = path.join(self.includes_dir, filename)
        # do not generate tuples if they are already present in codebase folder
        # tuples.h from codebase is copied into the includes folder
        if self.codebase:
            codebase_filepath = path.join(self.codebase, 'includes', filename)
            if path.isfile(codebase_filepath):
                copyfile(codebase_filepath, filepath)
                return


        # do not generate tuples if are already present or rewrite is false
        if path.isfile(filepath) and not rewrite:
            return

        # nodes = [self.source]
        # nodes.extend(self.internal_nodes)
        # nodes.append(self.sink)

        # Gathers all unique datatype
        tuples = set()
        tuples.add(self.datatype)
        for n in self.get_nodes():
            tuples.add(n.o_datatype)

        # Remove None object if any
        if None in tuples:
            tuples.remove(None)

        # Remove '' object if any
        if '' in tuples:
            tuples.remove('')

        # Generates all unique datatype
        result = ("#ifndef __TUPLES_H\n"
                  "#define __TUPLES_H\n")
        for t in tuples:
            result += ("typedef struct {\n"
                       "    uint key;\n"
                       "    float value;\n"
                       "} " + t + ";\n"
                       "\n"
                       "inline uint " + t + "_getKey(" + t + " data) {\n"
                       "    return data.key;\n"
                       "}\n"
                       "\n")
        result += "#endif //__TUPLES_H"
        file = open(filepath, mode='w+')
        file.write(result)
        file.close()

    def generate_functions(self,
                           rewrite=False):
        template = read_template_file(self.dest_dir, 'function.cl')

        # do not generate functions if they are already present in codebase folder
        # copy each function from codebase into the self.nodes_dir
        # not present functions will be generated
        if self.codebase:
            codebase_nodes_dir = path.join(self.codebase, 'device', 'nodes')
            for n in self.get_nodes():
                if n.has_functions():
                    filename = n.name + '.cl'
                    codebase_filepath = path.join(codebase_nodes_dir, filename)
                    filepath = path.join(self.nodes_dir, filename)
                    if path.isfile(codebase_filepath):
                        copyfile(codebase_filepath, filepath)
                    elif not path.isfile(filepath) or rewrite:
                        file = open(filepath, mode='w+')
                        result = template.render(node=n,
                                                 nodeKind=FNodeKind,
                                                 dispatchMode=FDispatchMode)
                        file.write(result)
                        file.close()
        else: #TODO RIVEDERE TUTTA LA FUNZIONE
            template = read_template_file(self.dest_dir, 'function.cl')
            for n in self.get_nodes():
                if n.has_functions():
                    filename = path.join(self.nodes_dir, n.name + '.cl')
                    if not path.isfile(filename) or rewrite:
                        file = open(filename, mode='w+')
                        result = template.render(node=n,
                                                 nodeKind=FNodeKind,
                                                 dispatchMode=FDispatchMode,
                                                 transfer_mode=self.transfer_mode,
                                                 transferMode=FTransferMode)
                        file.write(result)
                        file.close()

    def generate_device(self,
                        rewrite=False,
                        rewrite_device=False,
                        rewrite_functions=False,
                        rewirte_tuples=False):
        rewrite_device = rewrite or rewrite_device
        rewrite_functions = rewrite or rewrite_functions
        rewirte_tuples = rewrite or rewirte_tuples

        self.generate_fsp()
        self.generate_tuples(rewirte_tuples)
        self.generate_fsp_tuples()
        self.generate_functions(rewrite_functions)

        nodes = self.get_nodes()
        includes = [path.join(self.includes_dir, 'tuples.h')]

        # Creates functions
        node_functions = []
        for n in nodes:
            filename = n.name + '.cl'
            filepath = path.join(self.nodes_dir, filename)
            if path.isfile(filepath):
                if n.is_flat_map():
                    n.flat_map = generate_flat_map_code(n.name, filepath)
                else:
                    node_functions.append(filename)

        template = read_template_file('.', 'device.cl')
        result = template.render(nodeKind=FNodeKind,
                                 gatherKind=FGatherMode,
                                 dispatchKind=FDispatchMode,
                                 nodes=nodes,
                                 channels=self.channels,
                                 includes=includes,
                                 node_functions=node_functions,
                                 constants=self.constants | self.get_par_constants(),
                                 transfer_mode=self.transfer_mode,
                                 transferMode=FTransferMode)

        filename = path.join(self.device_dir, self.dest_dir + '.cl')
        if not path.isfile(filename) or rewrite_device:
            file = open(filename, mode='w+')
            file.write(result)
            file.close()

        # Removes 'flat_map' temporary files
        for n in nodes:
            if n.kind == FNodeKind.FLAT_MAP:
                if path.isfile(n.flat_map):
                    os.remove(n.flat_map)

    def generate_pipe(self, rewrite=False):
        nodes = self.get_nodes()
        buffers = []
        for n in nodes:
            for b in n.get_global_buffers():
                buffers.append(b)

        # TODO: avoid to check duplicates in all buffer names.
        # Do a proper naming for buffers instead

        # Checks duplicate buffer names
        buffers_set = set()
        for b in buffers:
            if b.name in buffers_set:
                sys.exit("Buffer's name '" + b.name + "'' already taken!")
            else:
                buffers_set.add(b.name)

        template = read_template_file(self.dest_dir, 'pipe.hpp')
        filename = path.join(self.includes_dir, 'pipe.hpp')
        if not path.isfile(filename) or rewrite:
            file = open(filename, mode='w+')
            result = template.render(nodes=self.internal_nodes,
                                     source=self.source,
                                     sink=self.sink,
                                     buffers=buffers,
                                     transfer_mode=self.transfer_mode,
                                     constants=self.constants | self.get_par_constants(),
                                     transferMode=FTransferMode,
                                     nodeKind=FNodeKind,
                                     bufferAccess=FBufferAccess)
            file.write(result)
            file.close()

    def generate_host(self,
                      rewrite=False,
                      rewrite_host=False,
                      rewrite_pipe=False):

        rewrite_host = rewrite or rewrite_host
        rewrite_pipe = rewrite or rewrite_pipe

        self.generate_pipe(rewrite_pipe)

        template = read_template_file(self.dest_dir, 'host.cpp')
        filename = path.join(self.host_dir, 'host.cpp')
        if not path.isfile(filename) or rewrite_host:
            file = open(filename, mode='w+')
            result = template.render(nodes=self.internal_nodes,
                                     source=self.source,
                                     sink=self.sink,
                                     transfer_mode=self.transfer_mode,
                                     transferMode=FTransferMode)
            file.write(result)
            file.close()

        common_dir = os.path.join(os.path.dirname(__file__), "src", 'common')
        files = ['buffers.hpp', 'common.hpp', 'ocl.hpp', 'opencl.hpp', 'utils.hpp']

        for f in files:
            src_path = path.join(common_dir, f)
            dest_path = path.join(self.common_dir, f)
            if not path.isfile(dest_path):
                copyfile(src_path, dest_path)

        # Makefile
        make_src_dir = os.path.join(os.path.dirname(__file__), "src", 'Makefile')
        make_dst_dir = path.join(self.base_dir, 'Makefile')

        if not path.isfile(make_dst_dir):
            copyfile(make_src_dir, make_dst_dir)
