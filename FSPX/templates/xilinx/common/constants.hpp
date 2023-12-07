#ifndef __CONSTANTS_HPP__
#define __CONSTANTS_HPP__

{% for key, value in constants.items() %}
#ifndef {{ key | upper }}
#define {{ '%-50s' | format((key | upper)) + (value | string) }}
#endif // {{ key | upper }}
{% endfor %}

{% for key, value in par_constants.items() %}
#ifndef {{ key | upper }}
#define {{ '%-50s' | format((key | upper)) + (value | string) }}
#endif // {{ key | upper }}

{% endfor %}

#endif // __CONSTANTS_HPP__