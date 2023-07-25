from enum import Enum


class FDispatchPolicy(Enum):
    NONE = 1
    RR = 2
    LB = 3
    KB = 4
    BR = 5

    def is_RR(self):
        return self == FDispatchPolicy.RR

    def is_LB(self):
        return self == FDispatchPolicy.LB

    def is_KB(self):
        return self == FDispatchPolicy.KB

    def is_BR(self):
        return self == FDispatchPolicy.BR
