from enum import Enum


class FDispatchMode(Enum):
    NONE = 1
    RR_BLOCKING = 2
    RR_NON_BLOCKING = 3
    KEYBY = 4
    BROADCAST = 5

    def is_RR(self):
        return self in (FDispatchMode.RR_BLOCKING, FDispatchMode.RR_NON_BLOCKING)

    def is_KEYBY(self):
        return self == FDispatchMode.KEYBY

    def is_BROADCAST(self):
        return self == FDispatchMode.BROADCAST
