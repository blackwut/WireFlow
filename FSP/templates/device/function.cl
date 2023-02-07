{% import 'source.cl' as source with context %}
{% import 'filter.cl' as filter with context %}
{% import 'map.cl' as map with context %}
{% import 'flat_map.cl' as flat_map with context %}
{% import 'sink.cl' as sink with context %}
{% import 'generator.cl' as generator with context %}
{% import 'collector.cl' as collector with context %}

{%- if node.is_source() %}
{{ source.declare_functions(node) }}
{% elif node.is_filter()%}
{{ filter.declare_functions(node) }}
{% elif node.is_map() %}
{{ map.declare_functions(node) }}
{% elif node.is_flat_map() %}
{{ flat_map.declare_functions(node) }}
{% elif node.is_sink() %}
{{ sink.declare_functions(node) }}
{% elif node.is_generator() %}
{{ generator.declare_functions(node) }}
{% elif node.is_collector() %}
{{ collector.declare_functions() }}
{% else %}
// Error in creating function
{% endif %}
