{%-macro keyby_lambda(op, next_op) %}
auto {{op.get_keyby_name()}} = [](const {{op.o_datatype}} & r) {
    return (int)(r.key % {{next_op.get_par_macro()}});
};
{%- endmacro %}

{% for op in nodes %}
{% if op.is_dispatch_KEYBY() %}
{{keyby_lambda(op, next_nodes[loop.index])}}
{% endif %}
{% endfor %}