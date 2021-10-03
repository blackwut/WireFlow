from enum import Enum


class FGatherMode(Enum):
    NONE = 1
    BLOCKING = 2
    NON_BLOCKING = 3

    def is_b(self):
        return self is FGatherMode.BLOCKING

    def is_nb(self):
        return self is FGatherMode.NON_BLOCKING
