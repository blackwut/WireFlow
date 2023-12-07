{% import 'source.cl' as source with context %}
{% import 'filter.cl' as filter with context %}
{% import 'map.cl' as map with context %}
{% import 'flat_map.cl' as flat_map with context %}
{% import 'memory_writer.cl' as memory_writer with context %}
{% import 'generator.cl' as generator with context %}
{% import 'drainer.cl' as drainer with context %}

{%- if node.is_memory_reader() %}
{{ memory_reader.declare_functions(node) }}
{% elif node.is_filter()%}
{{ filter.declare_functions(node) }}
{% elif node.is_map() %}
{{ map.declare_functions(node) }}
{% elif node.is_flat_map() %}
{{ flat_map.declare_functions(node) }}
{% elif node.is_memory_writer() %}
{{ memory_writer.declare_functions(node) }}
{% elif node.is_generator() %}
{{ generator.declare_functions(node) }}
{% elif node.is_drainer() %}
{{ drainer.declare_functions() }}
{% else %}
// Error in creating function
{% endif %}
