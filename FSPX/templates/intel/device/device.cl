{% import 'channel.cl' as ch with context %}
{% import 'utils.cl' as ut with context %}
{{ ch.enable_channels(channels) }}

{{ ut.decleare_defines(constants) }}

{{ ch.declare_channels(channels) }}

{{ ut.declare_flatmap_functions(nodes) }}

{{ ut.declare_nodes(nodes) }}