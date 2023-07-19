
#ifndef __CONSTANTS_H
#define __CONSTANTS_H

{% for key, value in constants.items() %}
#define {{ '%-50s' | format((key | upper)) + (value | string) }}
{% endfor %}

#endif // __CONSTANTS_H