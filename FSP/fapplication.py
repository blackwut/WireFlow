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
    SHARED = 2  # Intel only
    HOST = 3    # Xilinx only

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

        self.memory_reader = None
        self.internal_nodes = []
        self.memory_writer = None

        self.intel_generator = FGeneratorIntel(self)
        self.xilinx_generator = FGeneratorXilinx(self)

    def get_nodes(self):
        nodes = []
        if self.memory_reader:
            nodes.append(self.memory_reader)
        nodes.extend(self.internal_nodes)
        if self.memory_writer:
            nodes.append(self.memory_writer)
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
            self.intel_generator.generate_code(rewrite,
                                               rewrite_device,
                                               rewrite_functions,
                                               rewirte_tuples,
                                               rewrite_host,
                                               rewrite_pipe)
        elif self.target == FTarget.XILINX:
            self.xilinx_generator.generate_code(rewrite,
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

    def add(self,
            node: FOperator):
        assert node

        if node.is_memory_reader():
            if self.memory_reader:
                sys.exit("Memory reader already present!")
            self.memory_reader = node

        elif node.is_memory_writer():
            if self.memory_writer:
                sys.exit("Memory writer already present!")
            self.memory_writer = node

        elif node.is_generator():
            flag = False
            for n in self.internal_nodes:
                if n.is_generator():
                    flag = True
                    break
            if flag:
                sys.exit("Generator already present!")
            self.internal_nodes.insert(0, node)
        elif node.is_drainer():
            flag = False
            for n in self.internal_nodes:
                if n.is_drainer():
                    flag = True
                    break
            if flag:
                sys.exit("Drainer already present!")
            self.internal_nodes.append(node)

        elif node.kind in (FOperatorKind.MAP, FOperatorKind.FILTER, FOperatorKind.FLAT_MAP):
            flag = False
            for n in self.internal_nodes:
                if n.is_drainer():
                    flag = True
                    break
            if flag:
                self.internal_nodes.insert(-1, node)
            else:
                self.internal_nodes.append(node)
        else:
            sys.exit("Supplied node is not of type MEMORY_READER, MAP, FILTER, FLAT_MAP, MEMORY_WRITER, GENERATOR, or DRAINER")

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

        # nodes = [self.app.memory_reader]
        # nodes.extend(self.app.internal_nodes)
        # nodes.append(self.app.memory_writer)

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
        result = template.render(sink=self.app.memory_writer)
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

    def generate_memory_reader(self, rewrite=False):
        template = read_template_file(self.app.dest_dir, 'fsource.hpp')
        filename = path.join(self.app.host_includes_dir, 'fsource.hpp')
        if not path.isfile(filename) or rewrite:
            file = open(filename, mode='w+')
            result = template.render(source=self.app.memory_reader)
            file.write(result)
            file.close()

    def generate_memory_writer(self, rewrite=False):
        template = read_template_file(self.app.dest_dir, 'fsink.hpp')
        filename = path.join(self.app.host_includes_dir, 'fsink.hpp')
        if not path.isfile(filename) or rewrite:
            file = open(filename, mode='w+')
            result = template.render(sink=self.app.memory_writer)
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
                                     source=self.app.memory_reader,
                                     sink=self.app.memory_writer,
                                     buffers=buffers,
                                     transfer_mode=self.app.transfer_mode,
                                     constants=self.app.constants | self.get_par_constants(),
                                     transferMode=FTransferMode,
                                     nodeKind=FOperatorKind,
                                     bufferAccess=FBufferAccess)
            file.write(result)
            file.close()

        self.generate_memory_reader(rewrite)
        self.generate_memory_writer(rewrite)


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
        assert self.app.transfer_mode != FTransferMode.HOST

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
        assert self.app.transfer_mode != FTransferMode.HOST

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
                                     source=self.app.memory_reader,
                                     sink=self.app.memory_writer,
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
        ├── device
        │   └── includes
        │   └── nodes
        └── host
            └── includes
        """
        self.app.base_dir = self.app.dest_dir
        self.app.common_dir = path.join(self.app.base_dir, 'common')

        self.app.device_dir = path.join(self.app.base_dir, 'device')
        self.app.device_includes_dir = path.join(self.app.device_dir, 'includes')
        self.app.device_nodes_dir = path.join(self.app.device_dir, 'nodes')

        self.app.host_dir = path.join(self.app.base_dir, 'host')
        self.app.host_includes_dir = path.join(self.app.host_dir, 'includes')

        for folder in (self.app.base_dir, self.app.common_dir,
                       self.app.device_dir, self.app.device_includes_dir, self.app.device_nodes_dir,
                       self.app.host_dir, self.app.host_includes_dir):
            if not path.isdir(folder):
                os.mkdir(folder)

    def check_constraints(self):
        nodes = self.app.get_nodes()
        for n in nodes:
            if n.has_begin_function() or n.has_end_function():
                print(n.name + ": `begin` and `end` functions are not generated for Xilinx target")
            if n.is_memory_reader() or n.is_memory_writer():
                if n.has_compute_function():
                    print(n.name + ": compute function is not generated for Xilinx target")
            # if n.is_generator() or n.is_drainer():
            #     if n.has_compute_function():
            #         print(n.name + ": compute function is not generated for Xilinx target")


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

    def generate_constants(self, rewrite=False):
        filename = 'constants.hpp'

        # if 'constants.hpp' is already present in codebase folder
        if self.app.codebase:
            codebase_filepath = path.join(self.app.codebase, 'includes', filename)
            if rewrite and path.isfile(codebase_filepath):
                copyfile(codebase_filepath, path.join(self.app.common_dir, filename))
                return

        # otherwise generate it
        filepath = path.join(self.app.common_dir, filename)
        if rewrite or not path.isfile(filepath):
            template = read_template_file(self.app.dest_dir, filename, 'xilinx')
            result = template.render(constants=self.app.constants | self.get_par_constants())
            file = open(filepath, mode='w+')
            file.write(result)
            file.close()

    def generate_keyby_lambdas(self, rewrite=False):
        filename = "keyby_lambdas.hpp"

        # if 'keyby_lambdas.hpp' is already present in codebase folder
        if self.app.codebase:
            codebase_filepath = path.join(self.app.codebase, 'includes', filename)
            if rewrite and path.isfile(codebase_filepath):
                copyfile(codebase_filepath, path.join(self.app.common_dir, filename))
                return

        # otherwise generate it
        filepath = path.join(self.app.device_includes_dir, filename)
        if rewrite or not path.isfile(filepath):
            template = read_template_file(self.app.dest_dir, filename, 'xilinx')
            result = template.render(nodes=self.app.get_nodes())
            file = open(filepath, mode='w+')
            file.write(result)
            file.close()

    def generate_tuples(self, rewrite=False):
        tuples = self.get_tuples()
        template = read_template_file(self.app.dest_dir, 'tuple.hpp', 'xilinx')
        for tuple in tuples:
            filename = tuple + '.hpp'
            filepath = path.join(self.app.common_dir, filename)
            # if filename of corresponding tuple is already present in codebase folder
            if self.app.codebase:
                codebase_filepath = path.join(self.app.codebase, 'common', filename)
                if rewrite and path.isfile(codebase_filepath):
                    copyfile(codebase_filepath, filepath)
                    continue

            # otherwise generate it
            if rewrite or not path.isfile(filepath):
                result = template.render(tuple_name=tuple)
                file = open(filepath, mode='w+')
                file.write(result)
                file.close()

    def generate_functor_for(self, node, rewrite=False):
        # check if node is not a generator, drainer, memory reader or memory writer
        assert not node.is_generator() and not node.is_drainer() and not node.is_memory_reader() and not node.is_memory_writer()

        filename = node.name + '.hpp'

        # if filename of corresponding functor is already present in codebase folder
        if self.app.codebase:
            codebase_filepath = path.join(self.app.codebase, 'device', 'nodes', filename)
            if rewrite and path.isfile(codebase_filepath):
                copyfile(codebase_filepath, path.join(self.app.device_nodes_dir, filename))
                return

        # otherwise generate it
        filepath = path.join(self.app.device_nodes_dir, filename)
        if rewrite or not path.isfile(filepath):
            template = read_template_file(self.app.dest_dir, 'operator.hpp', 'xilinx')
            result = template.render(op=node)
            file = open(filepath, mode='w+')
            file.write(result)
            file.close()

    def generate_functions(self, rewrite=False):

        for node in self.app.internal_nodes:
            self.generate_functor_for(node, rewrite)
            # is_generated = False
            # filename = node.name + '.hpp'
            # filepath = path.join(self.app.device_nodes_dir, filename)

            # if self.app.codebase:
            #     codebase_filepath = path.join(self.app.codebase, 'device', 'nodes', filename)
            #     if rewrite and path.isfile(codebase_filepath):
            #         copyfile(codebase_filepath, filepath)
            #         is_generated = True

            # if not rewrite and path.isfile(filepath):
            #     is_generated = True

            # if not is_generated:
            #     self.generate_functor_for(node, rewrite)

    def generate_makefile(self, rewrite=False):
        filepath = path.join(self.app.base_dir, 'Makefile')
        if rewrite or not path.isfile(filepath):
            template = read_template_file(self.app.dest_dir, 'Makefile', 'xilinx')
            result = template.render(app_name=self.app.dest_dir,
                                     operators=self.app.get_nodes(),
                                     mr=self.app.memory_reader,
                                     mw=self.app.memory_writer,
                                     transfer_mode=self.app.transfer_mode,
                                     transferMode=FTransferMode)
            file = open(filepath, mode='w+')
            file.write(result)
            file.close()


################################################################################
#
# Device generation functions
#
################################################################################

    def generate_defines(self, rewrite=False):
        filename = path.join(self.app.device_includes_dir, 'defines.hpp')
        if rewrite or not path.isfile(filename):
            template = read_template_file(self.app.dest_dir, 'defines.hpp', 'xilinx')
            result = template.render(tuples=self.get_tuples(),
                                     mr=self.app.memory_reader,
                                     mw=self.app.memory_writer)
            file = open(filename, mode='w+')
            file.write(result)
            file.close()

    def generate_mr_kernel(self, rewrite=False):
        filename = path.join(self.app.device_dir, 'memory_reader.cpp')
        if rewrite or not path.isfile(filename):
            template = read_template_file(self.app.dest_dir, 'memory_reader.cpp', 'xilinx')
            result = template.render()
            file = open(filename, mode='w+')
            file.write(result)
            file.close()

    def generate_compute_kernel(self, rewrite=False):
        filename = path.join(self.app.device_dir, 'compute.cpp')
        if rewrite or not path.isfile(filename):
            template = read_template_file(self.app.dest_dir, 'compute.cpp', 'xilinx')
            result = template.render(operators=self.app.internal_nodes,
                                     nodes=self.app.get_nodes(),
                                     mr=self.app.memory_reader,
                                     mw=self.app.memory_writer)
            file = open(filename, mode='w+')
            file.write(result)
            file.close()

    def generate_mw_kernel(self, rewrite=False):
        filename = path.join(self.app.device_dir, 'memory_writer.cpp')
        if rewrite or not path.isfile(filename):
            template = read_template_file(self.app.dest_dir, 'memory_writer.cpp', 'xilinx')
            result = template.render()
            file = open(filename, mode='w+')
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

    def generate_host_includes(self, rewrite=False):
        if self.app.codebase:
            codebase_includes_dir = path.join(self.app.codebase, 'host', 'includes')
            for f in os.listdir(codebase_includes_dir):
                src_path = path.join(codebase_includes_dir, f)
                dest_path = path.join(self.app.host_includes_dir, f)
                if rewrite or not path.isfile(dest_path):
                    copyfile(src_path, dest_path)


    def generate_main(self, rewrite=False):
        filename = path.join(self.app.host_dir, 'host.cpp')
        if self.app.codebase:
            codebase_filepath = path.join(self.app.codebase, 'host', 'host.cpp')
            if rewrite and path.isfile(codebase_filepath):
                copyfile(codebase_filepath, filename)
                return

        if rewrite or not path.isfile(filename):
            template = read_template_file(self.app.dest_dir, 'host.cpp', 'xilinx')
            result = template.render(tuples=self.get_tuples(),
                                     mr=self.app.memory_reader,
                                     mw=self.app.memory_writer)
            file = open(filename, mode='w+')
            file.write(result)
            file.close()


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
        assert self.app.transfer_mode != FTransferMode.SHARED

        rewrite_device = rewrite or rewrite_device
        rewrite_functions = rewrite or rewrite_functions
        rewirte_tuples = rewrite or rewirte_tuples
        rewrite_keyby_lambdas = rewrite or rewrite_keyby_lambdas
        rewrite_any = rewrite or rewrite_device or rewrite_functions or rewirte_tuples

        self.check_constraints()
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
                      rewrite_host_includes=False,
                      rewrite_pipe=False):

        assert self.app.transfer_mode != FTransferMode.SHARED

        rewrite_host = rewrite or rewrite_host
        rewrite_host_includes = rewrite or rewrite_host_includes
        rewrite_pipe = rewrite or rewrite_pipe

        self.app.finalize()
        self.prepare_folders()

        self.generate_constants(rewrite)
        self.generate_host_includes(rewrite_host_includes)
        self.generate_main(rewrite_host)

    def generate_code(self,
                      rewrite=False,
                      rewrite_device=False,
                      rewrite_functions=False,
                      rewirte_tuples=False,
                      rewrite_host=False,
                      rewrite_host_includes=False,
                      rewrite_pipe=False,
                      rewrite_keyby_lambdas=False):
        self.generate_device(rewrite, rewrite_device, rewrite_functions, rewirte_tuples, rewrite_keyby_lambdas)
        self.generate_host(rewrite, rewrite_host, rewrite_host_includes, rewrite_pipe)
