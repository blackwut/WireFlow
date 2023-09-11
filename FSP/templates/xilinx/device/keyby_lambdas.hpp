{%-macro keyby_lambda(op) %}
auto {{op.get_keyby_lambda_name()}} = [](const {{op.o_datatype}} & r) {
    return (int)(r.key);
};
{%- endmacro %}

{% for op in nodes %}
{% if op.is_dispatch_KB() %}
{{keyby_lambda(op)}}
{% endif %}
{% endfor %}