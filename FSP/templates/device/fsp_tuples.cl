{% import 'channel.cl' as ch with context %}

{% macro declare_tuple(channel) -%}
{% set tupletype = channel.tupletype %}
{% set datatype  = channel.datatype %}
typedef struct {
    {{ datatype }} data;
    bool EOS;
} {{ tupletype }};

inline {{ tupletype }} create_{{ tupletype }}({{ datatype }} data)
{
    return ({{ tupletype }}){
                .data = data,
                .EOS = false
            };
}

inline {{ tupletype }} create_{{ tupletype }}_EOS()
{
    return ({{ tupletype }}){
                .data = {},
                .EOS = true
            };
}
{%- endmacro %}

{% for c in channels %}
{{ declare_tuple(c) }}
{% endfor %}

