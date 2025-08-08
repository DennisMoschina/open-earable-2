from _openearable import set_anc_filter, set_eq_filter, get_anc_sample_rate, get_eq_sample_rate
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

def set_anc_filter(filter_address: SlAddress, b0, b1, b2, a1, a2):
    """
    Set the ANC filter coefficients.
    Args:
        filter_address: SlAddress enum value indicating the filter slot
        b0, b1, b2: Coefficients for the numerator of the filter
        a1, a2: Coefficients for the denominator of the filter
    Returns:
        None
    Raises:
        ValueError: If coefficients are not in the expected range or format
    """
    coeffs = [b0, b1, b2, a1, a2]
    set_anc_filter(filter_address, coeffs)

def set_anc_filter(filter_type: FilterType, filter_slot: SlAddress, fc: float, g_dB: float, slope: float = 1):
    """
    Calculate and set the ANC filter coefficients.
    Args:
        filter_type: FilterType enum value indicating the type of filter
        filter_slot: SlAddress enum value indicating the filter slot
        fc: Cutoff frequency in Hz
        g_dB: Gain in dB (positive for boost, negative for cut)
        slope: Slope for shelving filters (default is 1)
    """
    b0, b1, b2, a1, a2 = _calculate_coeffs(filter_type, get_anc_sample_rate(), fc, g_dB, slope)
    set_anc_filter(filter_type, filter_slot, b0, b1, b2, a1, a2)

def set_eq_filter(filter_address: SlAddress, b0, b1, b2, a1, a2):
    """
    Set the EQ filter coefficients.
    Args:
        filter_address: SlAddress enum value indicating the filter slot
        b0, b1, b2: Coefficients for the numerator of the filter
        a1, a2: Coefficients for the denominator of the filter
    Returns:
        None
    Raises:
        ValueError: If coefficients are not in the expected range or format
    """
    coeffs = [b0, b1, b2, a1, a2]
    set_eq_filter(filter_address, coeffs)

def set_eq_filter(filter_type: FilterType, filter_slot: SlAddress, fc: float, g_dB: float, Q: float = None, slope: float = 1):
    """
    Calculate and set the EQ filter coefficients.
    Args:
        filter_type: FilterType enum value indicating the type of filter
        filter_slot: SlAddress enum value indicating the filter slot
        fc: Cutoff frequency in Hz (for shelves) or center frequency (for peaking)
        g_dB: Gain in dB (positive for boost, negative for cut)
        Q: Quality factor (required for peaking filters)
        slope: Slope for shelving filters (default is 1)"""
    b0, b1, b2, a1, a2 = _calculate_coeffs(filter_type, get_eq_sample_rate(), fc, g_dB, Q, slope)
    set_eq_filter(filter_type, filter_slot, b0, b1, b2, a1, a2)

_INT32_MIN = -2147483648
_INT32_MAX =  2147483647
_Q27 = 1 << 27  # Q5.27 scaling

def _clip_int32(x):
    if x < _INT32_MIN:
        return _INT32_MIN
    if x > _INT32_MAX:
        return _INT32_MAX
    return x

def _to_fixed_q27(coeffs):
    fixed = []
    for c in coeffs:
        x = c * _Q27
        # round-to-lower
        y = int(x)
        # clip to int32
        y = _clip_int32(y)
        fixed.append(y)
    return fixed

def _int32_to_hex(u):
    # Represent signed int32 as uint32 hex
    if u < 0:
        u = (u + (1 << 32)) & 0xFFFFFFFF
    return "0x%08X" % u

def _shelving_eq(fc, gain_db, fs, shelf_type, S=1.0):
    # shelf_type: 'low' or 'high'
    A = 10 ** (gain_db / 40.0)
    w0 = 2.0 * pi * fc / fs
    cosw0 = cos(w0)
    alpha = sin(w0) / 2.0 * sqrt((A + 1.0 / A) * (1.0 / S - 1.0) + 2.0)

    if shelf_type == FilterType.LOW_SHELF:
        b0 =    A * ((A + 1) - (A - 1) * cosw0 + 2 * sqrt(A) * alpha)
        b1 =  2*A * ((A - 1) - (A + 1) * cosw0)
        b2 =    A * ((A + 1) - (A - 1) * cosw0 - 2 * sqrt(A) * alpha)
        a0 =        (A + 1) + (A - 1) * cosw0 + 2 * sqrt(A) * alpha
        a1 =   -2 * ((A - 1) + (A + 1) * cosw0)
        a2 =        (A + 1) + (A - 1) * cosw0 - 2 * sqrt(A) * alpha
    elif shelf_type == FilterType.HIGH_SHELF:
        b0 =    A * ((A + 1) + (A - 1) * cosw0 + 2 * sqrt(A) * alpha)
        b1 = -2*A * ((A - 1) + (A + 1) * cosw0)
        b2 =    A * ((A + 1) + (A - 1) * cosw0 - 2 * sqrt(A) * alpha)
        a0 =        (A + 1) - (A - 1) * cosw0 + 2 * sqrt(A) * alpha
        a1 =    2 * ((A - 1) - (A + 1) * cosw0)
        a2 =        (A + 1) - (A - 1) * cosw0 - 2 * sqrt(A) * alpha
    else:
        raise ValueError("shelf_type must be 'low' or 'high'")

    # normalize so a0 = 1
    inv_a0 = 1.0 / a0
    b = [b0 * inv_a0, b1 * inv_a0, b2 * inv_a0]
    a = [1.0, a1 * inv_a0, a2 * inv_a0]
    return b, a

def _peaking_eq(f0, Q, gain_db, fs):
    A = 10 ** (gain_db / 40.0)
    omega = 2.0 * pi * f0 / fs
    cos_omega = cos(omega)
    alpha = sin(omega) / (2.0 * Q)

    b0 = 1 + alpha * A
    b1 = -2 * cos_omega
    b2 = 1 - alpha * A
    a0 = 1 + alpha / A
    a1 = -2 * cos_omega
    a2 = 1 - alpha / A

    inv_a0 = 1.0 / a0
    b = [b0 * inv_a0, b1 * inv_a0, b2 * inv_a0]
    a = [1.0, a1 * inv_a0, a2 * inv_a0]
    return b, a

def _calculate_coeffs(filter_type, fs, freq, gain_db=0.0, Q=None, S=1.0):
    """
    Compute biquad coefficients and Q5.27 fixed-point.

    Args:
        filter_type: 'low_shelf', 'high_shelf', or 'peaking'
        fs:          sampling rate (Hz)
        freq:        cutoff/center frequency (Hz)  (fc for shelves, f0 for peaking)
        gain_db:     dB gain (boost +, cut -)
        Q:           quality factor (peaking only)
        S:           shelf slope (shelves only), default 1.0

    Returns:
        b (list of float [b0,b1,b2]),
        a (list of float [1,a1,a2]),
        coeffs (list of float [b0,b1,b2,-a1,-a2]),
        fixed_q27 (list of int32 in Q5.27),
        fixed_hex (list of '0xXXXXXXXX' strings)
    """
    ft = filter_type
    if ft == FilterType.LOW_SHELF or ft == FilterType.HIGH_SHELF:
        b, a = _shelving_eq(freq, gain_db, fs, ft, S)
    elif ft == FilterType.PEAKING:
        if Q is None:
            raise ValueError("Q is required for 'peaking'")
        b, a = _peaking_eq(freq, Q, gain_db, fs)
    else:
        raise ValueError("filter_type must be 'low_shelf', 'high_shelf', or 'peaking'")

    # Pack like your MATLAB: [b0 b1 b2 -a1 -a2]
    coeffs = [b[0], b[1], b[2], -a[1], -a[2]]

    fixed_q27 = _to_fixed_q27(coeffs)
    fixed_hex = [_int32_to_hex(v) for v in fixed_q27]
    return fixed_hex
