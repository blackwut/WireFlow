import sys
import os
from os import path
from enum import Enum
from shutil import copyfile
from .futils import *
from .foperator import *
from .fchannel import *

# TODO: add check on all types (including FBuffer types) before create device/host files

class FTransferMode(Enum):
    COPY = 1
    SHARED = 2

class FTarget(Enum):
    INTEL = 1
    XILINX = 2

class FApplication:

    def __init__(self,
                 dest_dir: str,
                 datatype: str,
                 target: FTarget = FTarget.INTEL,
                 transfer_mode: FTransferMode = FTransferMode.COPY,
                 codebase: str = None,
                 constants: dict = {}):
        assert dest_dir
        assert datatype
        assert isinstance(target, FTarget)
        assert isinstance(transfer_mode, FTransferMode)
        assert codebase is None or path.isdir(codebase)
        assert isinstance(constants, dict)

        self.dest_dir = dest_dir
        self.datatype = datatype
        self.target = target
        self.transfer_mode = transfer_mode
        self.codebase = codebase
        self.constants = constants

        self.source = None
        self.internal_nodes = []
        self.sink = None

        self.intel_generator = FGeneratorIntel(self)
        self.xilinx_generator = FGeneratorXilinx(self)

    def get_nodes(self):
        nodes = []
        if self.source:
            nodes.append(self.source)
        nodes.extend(self.internal_nodes)
        if self.sink:
            nodes.append(self.sink)
        return nodes

    def generate_host(self,
                      rewrite=False,
                      rewrite_host=False,
                      rewrite_pipe=False):
        if self.target == FTarget.INTEL:
            self.intel_generator.generate_host(rewrite,
                                               rewrite_host,
                                               rewrite_pipe)
        elif self.target == FTarget.XILINX:
            self.xilinx_generator.generate_host(rewrite,
                                                rewrite_host,
                                                rewrite_pipe)
        else:
            sys.exit("Target not supported")

    def generate_device(self,
                        rewrite=False,
                        rewrite_device=False,
                        rewrite_functions=False,
                        rewirte_tuples=False,
                        rewrite_keyby_lambdas=False):
        if self.target == FTarget.INTEL:
            self.intel_generator.generate_device(rewrite,
                                                 rewrite_device,
                                                 rewrite_functions,
                                                 rewirte_tuples)
        elif self.target == FTarget.XILINX:
            self.xilinx_generator.generate_device(rewrite,
                                                  rewrite_device,
                                                  rewrite_functions,
                                                  rewirte_tuples,
                                                  rewrite_keyby_lambdas)
        else:
            sys.exit("Target not supported")

    def generate_code(self,
                    rewrite=False,
                    rewrite_device=False,
                    rewrite_functions=False,
                    rewirte_tuples=False,
                    rewrite_host=False,
                    rewrite_pipe=False,
                    rewrite_keyby_lambdas=False):
        if self.target == FTarget.INTEL:
            self.intel_generator(rewrite,
                                 rewrite_device,
                                 rewrite_functions,
                                 rewirte_tuples,
                                 rewrite_host,
                                 rewrite_pipe)
        elif self.target == FTarget.XILINX:
            self.xilinx_generator(rewrite,
                                  rewrite_device,
                                  rewrite_functions,
                                  rewirte_tuples,
                                  rewrite_host,
                                  rewrite_pipe,
                                  rewrite_keyby_lambdas)
        else:
            sys.exit("Target not supported")


################################################################################
#
# User's functions
#
################################################################################

    def add_source(self,
                   source: FOperator):
        assert source
        assert not self.source

        if source.kind == FOperatorKind.SOURCE:
            self.source = source
        else:
            sys.exit("Supplied node is not of type SOURCE")

    def add_sink(self,
                 sink: FOperator):
        assert sink
        assert not self.sink

        if sink.kind == FOperatorKind.SINK:
            self.sink = sink
        else:
            sys.exit("Supplied node is not of type SINK")

    def add(self,
            node: FOperator):
        assert node

        if node.kind not in (FOperatorKind.SOURCE, FOperatorKind.SINK):
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

        # # Creates folders
        # self.prepare_folders()


class FGeneratorIntel:

    def __init__(self,
                 app: FApplication):
        assert app
        self.app = app

    def get_par_constants(self):
        cs = {}
        nodes = self.app.get_nodes()
        for n in nodes:
            key = '__' + n.name.upper() + '_PAR'
            cs[key] = n.par
        return cs

    def prepare_folders(self):
        """
        app
        ├── common
        ├── ocl
        ├── device
        │   └── includes
        │   └── nodes
        └── host
            └── includes
            └── metric
        """
        self.app.base_dir = self.app.dest_dir
        self.app.common_dir = path.join(self.app.base_dir, 'common')

        self.app.ocl_dir = path.join(self.app.base_dir, 'ocl')

        self.app.device_dir = path.join(self.app.base_dir, 'device')
        self.app.device_includes_dir = path.join(self.app.device_dir, 'includes')
        self.app.device_nodes_dir = path.join(self.app.device_dir, 'nodes')

        self.app.host_dir = path.join(self.app.base_dir, 'host')
        self.app.host_includes_dir = path.join(self.app.host_dir, 'includes')
        self.app.host_metric_dir = path.join(self.app.host_dir, 'metric')

        for folder in (self.app.base_dir, self.app.common_dir, self.app.ocl_dir,
                       self.app.device_dir, self.app.device_includes_dir, self.app.device_nodes_dir,
                       self.app.host_dir, self.app.host_includes_dir, self.app.host_metric_dir):
            if not path.isdir(folder):
                os.mkdir(folder)

    def check_constraints(self):
        nodes = self.app.get_nodes()
        for n in nodes:
            if n.is_gather_KB():
                sys.exit("KB gather policy is not supported in Intel target")


################################################################################
#
# Common generation functions
#
################################################################################

    def generate_constants(self,
                           rewrite=False):
        filename = 'constants.h'
        template = read_template_file(self.app.dest_dir, filename)
        result = template.render(constants=self.app.constants | self.get_par_constants())

        filename = path.join(self.app.common_dir, filename)
        if not path.isfile(filename) or rewrite:
            file = open(filename, mode='w+')
            file.write(result)
            file.close()

    def generate_tuples(self,
                        rewrite=False):
        filename = 'tuples.h'
        filepath = path.join(self.app.common_dir, filename)
        # do not generate tuples if they are already present in codebase folder
        # tuples.h from codebase is copied into the includes folder
        if self.app.codebase:
            codebase_filepath = path.join(self.app.codebase, 'includes', filename)
            if path.isfile(codebase_filepath):
                copyfile(codebase_filepath, filepath)
                return


        # do not generate tuples if are already present or rewrite is false
        if path.isfile(filepath) and not rewrite:
            return

        # nodes = [self.app.source]
        # nodes.extend(self.app.internal_nodes)
        # nodes.append(self.app.sink)

        # Gathers all unique datatype
        tuples = set()
        tuples.add(self.app.datatype)
        for n in self.app.get_nodes():
            tuples.add(n.o_datatype)

        # Remove None object if any
        if None in tuples:
            tuples.remove(None)

        # Remove '' object if any
        if '' in tuples:
            tuples.remove('')

        # Generates all unique datatype
        result = ("#ifndef __TUPLES_H\n"
                  "#define __TUPLES_H\n"
                  "\n"
                  "#include \"constants.h\"\n")
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
        template = read_template_file(self.app.dest_dir, 'function.cl')

        # do not generate functions if they are already present in codebase folder
        # copy each function from codebase into the self.app.device_nodes_dir
        # not present functions will be generated
        if self.app.codebase:
            codebase_nodes_dir = path.join(self.app.codebase, 'device', 'nodes')
            for n in self.app.get_nodes():
                if n.has_functions():
                    filename = n.name + '.cl'
                    codebase_filepath = path.join(codebase_nodes_dir, filename)
                    filepath = path.join(self.app.device_nodes_dir, filename)
                    if path.isfile(codebase_filepath):
                        copyfile(codebase_filepath, filepath)
                    elif not path.isfile(filepath) or rewrite:
                        file = open(filepath, mode='w+')
                        result = template.render(node=n,
                                                 nodeKind=FOperatorKind,
                                                 dispatchPolicy=FDispatchPolicy,
                                                 transfer_mode=self.app.transfer_mode,
                                                 transferMode=FTransferMode)
                        file.write(result)
                        file.close()
        else: #TODO RIVEDERE TUTTA LA FUNZIONE
            template = read_template_file(self.app.dest_dir, 'function.cl')
            for n in self.app.get_nodes():
                if n.has_functions():
                    filename = path.join(self.app.device_nodes_dir, n.name + '.cl')
                    if not path.isfile(filename) or rewrite:
                        file = open(filename, mode='w+')
                        result = template.render(node=n,
                                                 nodeKind=FOperatorKind,
                                                 dispatchPolicy=FDispatchPolicy,
                                                 transfer_mode=self.app.transfer_mode,
                                                 transferMode=FTransferMode)
                        file.write(result)
                        file.close()


################################################################################
#
# Device generation functions
#
################################################################################

    def generate_fsp(self):
        filename = 'fsp.cl'
        filepath = path.join(self.app.device_includes_dir, filename)

        template = read_template_file(self.app.dest_dir, 'fsp.cl')
        file = open(filepath, mode='w+')
        result = template.render(sink=self.app.sink)
        file.write(result)
        file.close()

    def generate_fsp_tuples(self):
        filename = 'fsp_tuples.cl'
        filepath = path.join(self.app.device_includes_dir, filename)

        template = read_template_file(self.app.dest_dir, 'fsp_tuples.cl')
        file = open(filepath, mode='w+')
        result = template.render(channels=self.app.channels)
        file.write(result)
        file.close()

################################################################################
#
# Host generation functions
#
################################################################################

    def generate_fsource(self, rewrite=False):
        template = read_template_file(self.app.dest_dir, 'fsource.hpp')
        filename = path.join(self.app.host_includes_dir, 'fsource.hpp')
        if not path.isfile(filename) or rewrite:
            file = open(filename, mode='w+')
            result = template.render(source=self.app.source)
            file.write(result)
            file.close()

    def generate_fsink(self, rewrite=False):
        template = read_template_file(self.app.dest_dir, 'fsink.hpp')
        filename = path.join(self.app.host_includes_dir, 'fsink.hpp')
        if not path.isfile(filename) or rewrite:
            file = open(filename, mode='w+')
            result = template.render(sink=self.app.sink)
            file.write(result)
            file.close()

    def generate_pipe(self, rewrite=False):
        nodes = self.app.get_nodes()
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

        template = read_template_file(self.app.dest_dir, 'pipe.hpp')
        filename = path.join(self.app.host_includes_dir, 'pipe.hpp')
        if not path.isfile(filename) or rewrite:
            file = open(filename, mode='w+')
            result = template.render(nodes=self.app.internal_nodes,
                                     source=self.app.source,
                                     sink=self.app.sink,
                                     buffers=buffers,
                                     transfer_mode=self.app.transfer_mode,
                                     constants=self.app.constants | self.get_par_constants(),
                                     transferMode=FTransferMode,
                                     nodeKind=FOperatorKind,
                                     bufferAccess=FBufferAccess)
            file.write(result)
            file.close()

        self.generate_fsource(rewrite)
        self.generate_fsink(rewrite)


################################################################################
#
# User's functions
#
################################################################################

    def generate_device(self,
                        rewrite=False,
                        rewrite_device=False,
                        rewrite_functions=False,
                        rewirte_tuples=False):
        rewrite_device = rewrite or rewrite_device
        rewrite_functions = rewrite or rewrite_functions
        rewirte_tuples = rewrite or rewirte_tuples

        self.check_constraints()
        self.app.finalize()
        self.prepare_folders()

        self.generate_fsp()
        self.generate_tuples(rewirte_tuples)
        self.generate_fsp_tuples()
        self.generate_functions(rewrite_functions)

        nodes = self.app.get_nodes()
        includes = [path.join(self.app.common_dir, 'tuples.h')]

        # Creates functions
        node_functions = []
        for n in nodes:
            filename = n.name + '.cl'
            filepath = path.join(self.app.device_nodes_dir, filename)
            if path.isfile(filepath):
                if n.is_flat_map():
                    n.flat_map = generate_flat_map_code(n.name, filepath)
                else:
                    node_functions.append(filename)

        template = read_template_file('.', 'device.cl')
        result = template.render(nodeKind=FOperatorKind,
                                 nodes=nodes,
                                 channels=self.app.channels,
                                 includes=includes,
                                 node_functions=node_functions,
                                 constants=self.app.constants | self.get_par_constants(),
                                 transfer_mode=self.app.transfer_mode,
                                 transferMode=FTransferMode)

        filename = path.join(self.app.device_dir, self.app.dest_dir + '.cl')
        if not path.isfile(filename) or rewrite_device:
            file = open(filename, mode='w+')
            file.write(result)
            file.close()

        # Removes 'flat_map' temporary files
        for n in nodes:
            if n.kind == FOperatorKind.FLAT_MAP:
                if path.isfile(n.flat_map):
                    os.remove(n.flat_map)

    def generate_host(self,
                      rewrite=False,
                      rewrite_host=False,
                      rewrite_pipe=False):

        rewrite_host = rewrite or rewrite_host
        rewrite_pipe = rewrite or rewrite_pipe

        self.app.finalize()
        self.prepare_folders()

        self.generate_constants(rewrite)
        self.generate_pipe(rewrite_pipe)


        template_subpath = ("intel" if self.app.target == FTarget.INTEL else "xilinx")
        # Copy codebase files (host.cpp, dataset.hpp)
        # TODO: formalize which files are copied

        # HOST.CPP
        filename = path.join(self.app.codebase, 'host.cpp')
        if path.isfile(filename):
            copyfile(filename, path.join(self.app.host_dir, 'host.cpp'))

        # DATASET.HPP
        filename = path.join(self.app.codebase, 'includes', 'dataset.hpp')
        if path.isfile(filename):
            copyfile(filename, path.join(self.app.host_includes_dir, 'dataset.hpp'))

        template = read_template_file(self.app.dest_dir, 'host.cpp')
        filename = path.join(self.app.host_dir, 'host.cpp')
        if not path.isfile(filename) or rewrite_host:
            file = open(filename, mode='w+')
            result = template.render(nodes=self.app.internal_nodes,
                                     source=self.app.source,
                                     sink=self.app.sink,
                                     transfer_mode=self.app.transfer_mode,
                                     transferMode=FTransferMode)
            file.write(result)
            file.close()

        # OCL
        ocl_dir = os.path.join(os.path.dirname(__file__), "src", template_subpath, 'ocl')
        files = ['fbuffers.hpp', 'ocl.hpp', 'opencl.hpp', 'utils.hpp']

        for f in files:
            src_path = path.join(ocl_dir, f)
            dest_path = path.join(self.app.ocl_dir, f)
            if not path.isfile(dest_path):
                copyfile(src_path, dest_path)

        # Metric
        metric_dir = os.path.join(os.path.dirname(__file__), "src", template_subpath, 'metric')
        files = ['metric_group.hpp', 'metric.hpp', 'sampler.hpp']

        for f in files:
            src_path = path.join(metric_dir, f)
            dest_path = path.join(self.app.host_metric_dir, f)
            if not path.isfile(dest_path):
                copyfile(src_path, dest_path)

        # Makefile
        make_src_dir = os.path.join(os.path.dirname(__file__), "src", template_subpath, 'Makefile')
        make_dst_dir = path.join(self.app.base_dir, 'Makefile')

        if not path.isfile(make_dst_dir):
            copyfile(make_src_dir, make_dst_dir)

    def generate_code(self,
                      rewrite=False,
                      rewrite_device=False,
                      rewrite_functions=False,
                      rewirte_tuples=False,
                      rewrite_host=False,
                      rewrite_pipe=False):
        self.app.finalize()
        self.prepare_folders()
        self.generate_device(rewrite, rewrite_device, rewrite_functions, rewirte_tuples)
        self.generate_host(rewrite, rewrite_host, rewrite_pipe)


class FGeneratorXilinx:

    def __init__(self,
                 app: FApplication):
        assert app
        self.app = app

    def get_par_constants(self):
        cs = {}
        nodes = self.app.get_nodes()
        for n in nodes:
            key = n.get_par_macro()
            cs[key] = n.par
        return cs

    def prepare_folders(self):
        """
        app
        ├── common
        ├── ocl
        ├── device
        │   └── includes
        │   └── nodes
        └── host
            └── includes
            └── metric
        """
        self.app.base_dir = self.app.dest_dir
        self.app.common_dir = path.join(self.app.base_dir, 'common')

        self.app.ocl_dir = path.join(self.app.base_dir, 'ocl')

        self.app.device_dir = path.join(self.app.base_dir, 'device')
        self.app.device_includes_dir = path.join(self.app.device_dir, 'includes')
        self.app.device_nodes_dir = path.join(self.app.device_dir, 'nodes')

        self.app.host_dir = path.join(self.app.base_dir, 'host')
        self.app.host_includes_dir = path.join(self.app.host_dir, 'includes')
        self.app.host_metric_dir = path.join(self.app.host_dir, 'metric')

        for folder in (self.app.base_dir, self.app.common_dir, self.app.ocl_dir,
                       self.app.device_dir, self.app.device_includes_dir, self.app.device_nodes_dir,
                       self.app.host_dir, self.app.host_includes_dir, self.app.host_metric_dir):
            if not path.isdir(folder):
                os.mkdir(folder)


################################################################################
#
# Common generation functions
#
################################################################################

    def get_tuples(self):
        tuples = set()
        tuples.add(self.app.datatype)
        for n in self.app.get_nodes():
            tuples.add(n.o_datatype)

        # Remove None object if any
        if None in tuples:
            tuples.remove(None)

        # Remove '' object if any
        if '' in tuples:
            tuples.remove('')

        return tuples

    def generate_constants(self,
                           rewrite=False):
        filename = 'constants.hpp'
        template = read_template_file(self.app.dest_dir, filename, 'xilinx')
        result = template.render(constants=self.app.constants | self.get_par_constants())

        filepath = path.join(self.app.common_dir, filename)
        if not path.isfile(filepath) or rewrite:
            file = open(filepath, mode='w+')
            file.write(result)
            file.close()

    def generate_keyby_lambdas(self, rewrite=False):
        filename = "keyby_lambdas.hpp"
        template = read_template_file(self.app.dest_dir, filename, 'xilinx')
        filepath = path.join(self.app.device_includes_dir, filename)
        if not path.isfile(filepath) or rewrite:
            file = open(filepath, mode='w+')
            result = template.render(nodes=self.app.get_nodes()[:-1], next_nodes=self.app.get_nodes()[1:])
            file.write(result)
            file.close()

    def generate_tuples(self,
                        rewrite=False):

        template = read_template_file(self.app.dest_dir, 'tuple.hpp', 'xilinx')
        tuples = self.get_tuples()

        for tuple in tuples:
            filename = tuple + '.hpp'
            filepath = path.join(self.app.common_dir, filename)
            # do not generate tuples if they are already present in codebase folder
            # tuples.h from codebase is copied into the includes folder
            if self.app.codebase:
                codebase_filepath = path.join(self.app.codebase, 'includes', filename)
                if path.isfile(codebase_filepath):
                    copyfile(codebase_filepath, filepath)
                    return

            # do not generate tuples if are already present or rewrite is false
            if path.isfile(filepath) and not rewrite:
                return

            file = open(filepath, mode='w+')
            result = template.render(tuple_name=tuple)
            file.write(result)
            file.close()

    def generate_functor_for(self, node):
        template = read_template_file(self.app.dest_dir, 'operator.hpp', 'xilinx')
        filename = node.name + '.hpp'
        filepath = path.join(self.app.device_nodes_dir, filename)

        file = open(filepath, mode='w+')
        result = template.render(op=node)
        file.write(result)
        file.close()

    def generate_functions(self,
                           rewrite=False):

        for node in self.app.internal_nodes:
            is_generated = False
            filename = node.name + '.cpp'
            filepath = path.join(self.app.device_nodes_dir, filename)

            if self.app.codebase:
                codebase_filepath = path.join(self.app.codebase, 'device', 'nodes', filename)
                if path.isfile(codebase_filepath):
                    copyfile(codebase_filepath, filepath)
                    is_generated = True

            if path.isfile(filepath) and not rewrite:
                is_generated = True

            if not is_generated:
                self.generate_functor_for(node)

    def generate_makefile(self, rewrite=False):
        template = read_template_file(self.app.dest_dir, 'Makefile', 'xilinx')
        filepath = path.join(self.app.base_dir, 'Makefile')
        if not path.isfile(filepath) or rewrite:
            file = open(filepath, mode='w+')
            result = template.render(operators=self.app.get_nodes(),
                                     mr=self.app.source,
                                     mw=self.app.sink)
            file.write(result)
            file.close()


################################################################################
#
# Device generation functions
#
################################################################################

    def generate_defines(self, rewrite=False):
        template = read_template_file(self.app.dest_dir, 'defines.hpp', 'xilinx')
        filename = path.join(self.app.device_includes_dir, 'defines.hpp')

        if not path.isfile(filename) or rewrite:
            tuples = self.get_tuples()
            file = open(filename, mode='w+')
            result = template.render(tuples=tuples,
                                     mr=self.app.source,
                                     mw=self.app.sink,
                                     datatypes=self.get_tuples())
            file.write(result)
            file.close()

    def generate_mr_kernel(self, rewrite=False):
        template = read_template_file(self.app.dest_dir, 'memory_reader.cpp', 'xilinx')
        filename = path.join(self.app.device_dir, 'memory_reader.cpp')
        if not path.isfile(filename) or rewrite:
            file = open(filename, mode='w+')
            result = template.render()
            file.write(result)
            file.close()

    def generate_compute_kernel(self, rewrite=False):
        template = read_template_file(self.app.dest_dir, 'compute.cpp', 'xilinx')
        filename = path.join(self.app.device_dir, 'compute.cpp')
        if not path.isfile(filename) or rewrite:
            file = open(filename, mode='w+')
            result = template.render(operators=self.app.internal_nodes,
                                     nodes=self.app.get_nodes(),
                                     mr=self.app.source,
                                     mw=self.app.sink)
            file.write(result)
            file.close()

    def generate_mw_kernel(self, rewrite=False):
        template = read_template_file(self.app.dest_dir, 'memory_writer.cpp', 'xilinx')
        filename = path.join(self.app.device_dir, 'memory_writer.cpp')
        if not path.isfile(filename) or rewrite:
            file = open(filename, mode='w+')
            result = template.render()
            file.write(result)
            file.close()

    def generate_kernels(self, rewrite=False):
        self.generate_mr_kernel(rewrite)
        self.generate_compute_kernel(rewrite)
        self.generate_mw_kernel(rewrite)

################################################################################
#
# Host generation functions
#
################################################################################

    def generate_fsource(self, rewrite=False):
        template = read_template_file(self.app.dest_dir, 'fsource.hpp')
        filename = path.join(self.app.host_includes_dir, 'fsource.hpp')
        if not path.isfile(filename) or rewrite:
            file = open(filename, mode='w+')
            result = template.render(source=self.app.source)
            file.write(result)
            file.close()

    def generate_fsink(self, rewrite=False):
        template = read_template_file(self.app.dest_dir, 'fsink.hpp')
        filename = path.join(self.app.host_includes_dir, 'fsink.hpp')
        if not path.isfile(filename) or rewrite:
            file = open(filename, mode='w+')
            result = template.render(sink=self.app.sink)
            file.write(result)
            file.close()

    def generate_pipe(self, rewrite=False):
        nodes = self.app.get_nodes()
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

        template = read_template_file(self.app.dest_dir, 'pipe.hpp')
        filename = path.join(self.app.host_includes_dir, 'pipe.hpp')
        if not path.isfile(filename) or rewrite:
            file = open(filename, mode='w+')
            result = template.render(nodes=self.app.internal_nodes,
                                     source=self.app.source,
                                     sink=self.app.sink,
                                     buffers=buffers,
                                     transfer_mode=self.app.transfer_mode,
                                     constants=self.app.constants | self.get_par_constants(),
                                     transferMode=FTransferMode,
                                     nodeKind=FOperatorKind,
                                     bufferAccess=FBufferAccess)
            file.write(result)
            file.close()

        self.generate_fsource(rewrite)
        self.generate_fsink(rewrite)


################################################################################
#
# User's functions
#
################################################################################

    def generate_device(self,
                        rewrite=False,
                        rewrite_device=False,
                        rewrite_functions=False,
                        rewirte_tuples=False,
                        rewrite_keyby_lambdas=False):
        rewrite_device = rewrite or rewrite_device
        rewrite_functions = rewrite or rewrite_functions
        rewirte_tuples = rewrite or rewirte_tuples
        rewrite_any = rewrite or rewrite_device or rewrite_functions or rewirte_tuples

        self.app.finalize()
        self.prepare_folders()

        self.generate_tuples(rewirte_tuples)
        self.generate_defines(rewirte_tuples)
        self.generate_constants(rewrite_any)
        self.generate_keyby_lambdas(rewrite_keyby_lambdas)

        self.generate_functions(rewrite_functions)
        self.generate_kernels(rewrite_device)
        self.generate_makefile(rewrite_any)

    def generate_host(self,
                      rewrite=False,
                      rewrite_host=False,
                      rewrite_pipe=False):

        rewrite_host = rewrite or rewrite_host
        rewrite_pipe = rewrite or rewrite_pipe

        self.app.finalize()
        self.prepare_folders()

        self.generate_constants(rewrite)
        # self.generate_pipe(rewrite_pipe)

        print("generate_host not implemented for Xilinx")


        # template_subpath = ("intel" if self.app.target == FTarget.INTEL else "xilinx")
        # # Copy codebase files (host.cpp, dataset.hpp)
        # # TODO: formalize which files are copied

        # # HOST.CPP
        # filename = path.join(self.app.codebase, 'host.cpp')
        # if path.isfile(filename):
        #     copyfile(filename, path.join(self.app.host_dir, 'host.cpp'))

        # # DATASET.HPP
        # filename = path.join(self.app.codebase, 'includes', 'dataset.hpp')
        # if path.isfile(filename):
        #     copyfile(filename, path.join(self.app.host_includes_dir, 'dataset.hpp'))

        # template = read_template_file(self.app.dest_dir, 'host.cpp')
        # filename = path.join(self.app.host_dir, 'host.cpp')
        # if not path.isfile(filename) or rewrite_host:
        #     file = open(filename, mode='w+')
        #     result = template.render(nodes=self.app.internal_nodes,
        #                              source=self.app.source,
        #                              sink=self.app.sink,
        #                              transfer_mode=self.app.transfer_mode,
        #                              transferMode=FTransferMode)
        #     file.write(result)
        #     file.close()

        # # OCL
        # ocl_dir = os.path.join(os.path.dirname(__file__), "src", template_subpath, 'ocl')
        # files = ['fbuffers.hpp', 'ocl.hpp', 'opencl.hpp', 'utils.hpp']

        # for f in files:
        #     src_path = path.join(ocl_dir, f)
        #     dest_path = path.join(self.app.ocl_dir, f)
        #     if not path.isfile(dest_path):
        #         copyfile(src_path, dest_path)

        # # Metric
        # metric_dir = os.path.join(os.path.dirname(__file__), "src", template_subpath, 'metric')
        # files = ['metric_group.hpp', 'metric.hpp', 'sampler.hpp']

        # for f in files:
        #     src_path = path.join(metric_dir, f)
        #     dest_path = path.join(self.app.host_metric_dir, f)
        #     if not path.isfile(dest_path):
        #         copyfile(src_path, dest_path)

        # # Makefile
        # make_src_dir = os.path.join(os.path.dirname(__file__), "src", template_subpath, 'Makefile')
        # make_dst_dir = path.join(self.app.base_dir, 'Makefile')

        # if not path.isfile(make_dst_dir):
        #     copyfile(make_src_dir, make_dst_dir)

    def generate_code(self,
                      rewrite=False,
                      rewrite_device=False,
                      rewrite_functions=False,
                      rewirte_tuples=False,
                      rewrite_host=False,
                      rewrite_pipe=False,
                      rewrite_keyby_lambdas=False):
        # self.app.finalize()
        # self.prepare_folders()
        # self.generate_device(rewrite, rewrite_device, rewrite_functions, rewirte_tuples)
        # self.generate_host(rewrite, rewrite_host, rewrite_pipe)
        sys.exit("generate_code not implemented for Xilinx")