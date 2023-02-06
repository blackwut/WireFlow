# Utilities Module
import sys
import os
import logging
import jinja2 as jinja
import re


def previous_current_next(iterable):
    iterable = iter(iterable)
    prv = None
    cur = iterable.__next__()
    try:
        while True:
            nxt = iterable.__next__()
            yield (prv, cur, nxt)
            prv = cur
            cur = nxt
    except StopIteration:
        yield (prv, cur, None)


def generate_flat_map_code(node_name, filename):

    # Reads flat_map code
    file = open(filename, mode='r')
    code = file.read()
    file.close()

    # Replaces '__SEND(var)' with jinja2 code
    if len(re.findall(r'__SEND\(', code, re.MULTILINE)) > 1:
        sys.exit(node_name + ': Multiple "__SEND()" call functions are not allowed!')

    regex = r'(\s*__SEND\()\s*(\S+)\s*(\);)'
    result = re.search(regex, code, re.MULTILINE)
    var_name = result.group(2)
    start_pos = result.span(0)[0]
    end_pos = result.span(0)[1]

    new_text = ("\n\n"
                "        {{ node.create_o_tuple('t_out', '" + var_name + "') }};\n"
                "        {{ ch.dispatch_tuple(node, 'idx', 'w', 't_out', true) | indent(8)}}")

    new_code = code[:start_pos] + new_text + code[end_pos:]

    # Writes a new temporary file
    new_filename = filename + '.tmp'
    with open(new_filename, 'w+') as f:
        f.write(new_code)

    return new_filename


def read_template_file(source_code_dir, path):
    templates = [os.path.join(os.path.dirname(__file__), "templates"),
                 os.path.join(os.path.dirname(__file__), "templates", "device"),
                 os.path.join(os.path.dirname(__file__), "templates", "host"),
                 source_code_dir]
    loader = jinja.FileSystemLoader(searchpath=templates) # source_code_dir is needed for flat_map operator?
    # sources_tmp = os.path.join(sources, 'tmp')
    # loader = jinja.FileSystemLoader(searchpath=[templates, sources, sources_tmp])

    logging.basicConfig()
    logger = logging.getLogger('logger')
    logger = jinja.make_logging_undefined(logger=logger, base=jinja.Undefined)

    env = jinja.Environment(loader=loader, undefined=logger)
    env.lstrip_blocks = True
    env.trim_blocks = True
    env.filters['generate_flat_map'] = generate_flat_map_code
    return env.get_template(path)
