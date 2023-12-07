from enum import Enum


class FGatherPolicy(Enum):
    NONE = 1
    RR = 2
    LB = 3
    KB = 4

    def is_RR(self):
        return self is FGatherPolicy.RR

    def is_LB(self):
        return self is FGatherPolicy.LB

    def is_KB(self):
        return self is FGatherPolicy.KB
