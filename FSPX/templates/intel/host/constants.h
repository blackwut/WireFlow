#ifndef __CONSTANTS_H
#define __CONSTANTS_H

{% for key, value in constants.items() %}
#ifndef {{ key | upper }}
#define {{ '%-50s' | format((key | upper)) + (value | string) }}
#endif // {{ key | upper }}
{% endfor %}

#endif // __CONSTANTS_H