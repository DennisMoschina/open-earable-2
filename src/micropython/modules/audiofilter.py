from _openearable import set_anc_filter, set_eq_filter
from math import sin, cos, pi, sqrt

class SlAddress:
    BIQ_0 = 0
    EXPANDER = 5
    VOLUME = 6
    MUTE = 7
    MIXER = 8
    LIMITER_MASTER = 9

class FilterType:
    LOW_SHELF = 'low_shelf'
    HIGH_SHELF = 'high_shelf'
    PEAKING = 'peaking'

def set_anc_filter(filter_address: SlAddress, b1, b2, b3, a1, a2):
    """Set the ANC filter coefficients."""
    coeffs = [b1, b2, b3, a1, a2]
    set_anc_filter(filter_address, coeffs)
