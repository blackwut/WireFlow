#ifndef __CONSTANTS_HPP__
#define __CONSTANTS_HPP__

{% for key, value in constants.items() %}
#define {{ '%-50s' | format((key | upper)) + (value | string) }}
{% endfor %}

#endif // __CONSTANTS_HPP__