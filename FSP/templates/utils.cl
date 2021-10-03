{% import 'channel.cl' as ch with context %}
{% import 'source.cl' as source with context %}
{% import 'filter.cl' as filter with context %}
{% import 'map.cl' as map with context %}
{% import 'flat_map.cl' as flat_map with context %}
{% import 'sink.cl' as sink with context %}
{% import 'generator.cl' as generator with context %}
{% import 'collector.cl' as collector with context %}

{% macro decleare_defines(constants) -%}
#include "includes/fsp.cl"

{% for key, value in constants.items() %}
#define {{ '%-50s' | format((key | upper)) + (value | string) }}
{% endfor %}

#include "../includes/tuples.h"
#include "includes/fsp_tuples.cl"
{% for f in node_functions %}
#include "nodes/{{ f }}"
{% endfor %}
{%- endmacro %}

{% macro declare_flatmap_functions(nodes) -%}
{% for node in nodes %}
{% if node.is_flat_map() %}
{% include node.flat_map with context %}
{% endif %}
{% endfor %}
{%- endmacro %}


{% macro declare_nodes(nodes) -%}
{% for node in nodes %}
{% for idx in range(node.par) %}
{% if node.is_source() %}
{{ source.node(node, idx) }}
{% elif node.is_filter() %}
{{ filter.node(node, idx) }}
{% elif node.is_map() %}
{{ map.node(node, idx) }}
{% elif node.is_flat_map() %}
{{ flat_map.node(node, idx) }}
{% elif node.is_sink() %}
{{ sink.node(node, idx) }}
{% elif node.is_generator() %}
{{ generator.node(node, idx) }}
{% elif node.is_collector() %}
{{ collector.node(node, idx) }}
{% else %}
{% endif %}

{% endfor %}
{% endfor %}
{%- endmacro %}
