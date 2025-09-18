import openearable as oe
import processing as proc
from processing import Source, Sink
import time
import app_core

app_info = app_core.AppInfo("Heart Rate App", "A simple heart rate app using PPG data", app_core.AppType.DAEMON)

# =======================
# Tiny, streaming state
# =======================
# (All timestamps are µs)

# Tunables
_MIN_PERIOD_US   = 260_000     # ~230 bpm upper bound
_MAX_PERIOD_US   = 2_000_000   # ~30 bpm lower bound
_REFRACTORY_US   = 280_000     # ignore closely spaced ZCs (ringing)
_MISSED_FACTOR   = 1.75        # >1.75x median → assume one missed beat (split)
_SHORT_FACTOR    = 0.5         # <0.5x median → drop as duplicate/ringing
_PROM_FRAC       = 0.30        # peak must be >= frac * median prominence
IBI_WIN          = 9           # small sorted IBI buffer
PROM_WIN         = 15          # small sorted prominence buffer

# Globals (kept minimal)
is_in_ear = False
_last_neg_zc_ts = None
_last_counted_neg_zc_ts = None      # for refractory
_had_good_peak_since_last_negzc = False

_ibi_sorted = []     # sorted list of recent IBIs (µs), max len IBI_WIN
_prom_sorted = []    # sorted list of recent prominences, max len PROM_WIN

# =======================
# Small helpers (no imports)
# =======================
def _insert_sorted(buf, val, maxlen):
    # insertion sort into small list; keeps ascending order
    n = len(buf)
    # fast paths
    if n == 0:
        buf.append(val)
        return
    if val >= buf[-1]:
        buf.append(val)
    else:
        i = 0
        while i < n and buf[i] <= val:
            i += 1
        buf.insert(i, val)
    if len(buf) > maxlen:
        # drop the farthest outlier (choose side by distance to median)
        # but to keep it simple & cheap: pop from ends evenly
        # prefer removing extremes—remove whichever end is farther from current median
        med = _median(buf)
        if abs(buf[0] - med) > abs(buf[-1] - med):
            buf.pop(0)
        else:
            buf.pop()

def _median(buf):
    n = len(buf)
    if n == 0:
        return None
    m = n >> 1      # n//2 (but very fast)
    if n & 1:
        return buf[m]
    return (buf[m - 1] + buf[m]) / 2.0

def _clear_state_from(ts_cutoff_us):
   _ibi_sorted.clear()
   _prom_sorted.clear()

def reset(ts):
    # on re-insert ear, reset minimal beat sequence state but keep windows
    global _last_neg_zc_ts, _last_counted_neg_zc_ts, _had_good_peak_since_last_negzc
    _last_neg_zc_ts = None
    _last_counted_neg_zc_ts = None
    _had_good_peak_since_last_negzc = False
    _clear_state_from(ts)

# =======================
# Sinks (tiny, allocation-free)
# =======================
def detect_inear(sensor_value):
    # Using SwitchStage output: component[1].value > 0 means in-ear
    global is_in_ear
    is_in_ear = sensor_value.groups[0].components[1].value > 0
    print("Detected {} ear with skin temp {:.1f}C".format("in" if is_in_ear else "out of", sensor_value.groups[0].components[0].value))
    if is_in_ear:
        reset(sensor_value.timestamp)

def fill_peaks(sensor_value):
    # PeakDetector output convention:
    #   components[1] = sign (+1 / -1)
    #   components[2] = prominence (float)
    if not is_in_ear:
        return
    prom = sensor_value.groups[0].components[2].value
    # maintain rolling sorted prominence buffer
    _insert_sorted(_prom_sorted, prom, PROM_WIN)

    # if peak is strong enough compared to recent median, arm the “good peak” flag
    med_prom = _median(_prom_sorted)
    if (med_prom is None) or (prom >= _PROM_FRAC * med_prom):
        # peak occurred and is good → allow next negative ZC to close a beat
        # (we don’t care about sign here; either can indicate a physiologic peak)
        global _had_good_peak_since_last_negzc
        _had_good_peak_since_last_negzc = True

def fill_zero_crossings(sensor_value):
    # ZeroCrossingDetector output:
    #   components[0] = value (+1 or -1)
    if not is_in_ear:
        return

    val = sensor_value.groups[0].components[0].value
    if val >= 0:
        return  # only use negative crossings for beat boundaries

    ts = sensor_value.timestamp

    global _last_counted_neg_zc_ts
    if _last_counted_neg_zc_ts is not None:
        if (ts - _last_counted_neg_zc_ts) < _REFRACTORY_US:
            return  # refractory: suppress ringing

    global _last_neg_zc_ts, _had_good_peak_since_last_negzc

    # Need a good peak between successive neg ZCs to accept this beat
    if not _had_good_peak_since_last_negzc:
        _last_neg_zc_ts = ts
        _last_counted_neg_zc_ts = ts
        return

    if _last_neg_zc_ts is None:
        _last_neg_zc_ts = ts
        _last_counted_neg_zc_ts = ts
        _had_good_peak_since_last_negzc = False
        return

    dt = ts - _last_neg_zc_ts
    # period sanity
    if dt < _MIN_PERIOD_US or dt > _MAX_PERIOD_US:
        _last_neg_zc_ts = ts
        _last_counted_neg_zc_ts = ts
        _had_good_peak_since_last_negzc = False
        return

    # Base median IBI (use current window; if empty, seed with dt)
    med = _median(_ibi_sorted)
    if med is None:
        _insert_sorted(_ibi_sorted, dt, IBI_WIN)
        _last_neg_zc_ts = ts
        _last_counted_neg_zc_ts = ts
        _had_good_peak_since_last_negzc = False
        # Need a few beats before reporting
        return

    # Missed-beat & short-duplicate handling
    if dt > _MISSED_FACTOR * med and (dt >> 1) >= _MIN_PERIOD_US:
        # split into two beats
        half = dt / 2.0
        _insert_sorted(_ibi_sorted, half, IBI_WIN)
        _insert_sorted(_ibi_sorted, half, IBI_WIN)
    elif dt < _SHORT_FACTOR * med:
        # likely duplicate / ringing → drop
        pass
    else:
        _insert_sorted(_ibi_sorted, dt, IBI_WIN)

    # Compute BPM from robust central tendency:
    # Use median; optionally a tiny trimmed mean around it without allocations
    med = _median(_ibi_sorted)
    if med is not None:
        bpm = 60.0 * 1_000_000.0 / med
        # print ONLY BPM int
        print("HR: {:3d} bpm".format(int(bpm)))

    _last_neg_zc_ts = ts
    _last_counted_neg_zc_ts = ts
    _had_good_peak_since_last_negzc = False

# =======================
# Main (setup pipelines, run forever)
# =======================
def main():
    # minimal prints to reduce heap churn
    oe.init_sensors()
    ppg = oe.get_sensor(4)
    skin_temp = [s for s in oe.get_sensors() if s.name == "Skin Temperature Sensor"][0]

    p = proc.Pipeline('HR')
    p.source('ppg', ppg)
    p.stage(proc.ComponentExtractor('green_ex', group='PHOTOPLETHYSMOGRAPHY', component='GREEN'))
    p.connect('ppg', 'green_ex')
    p.stage(proc.BiQuadFilter('filter', stages=2, coeffs=[
        [ 0.01658193,  0.03316386,  0.01658193, -1.62885077,  0.69946348],
        [ 1.0,        -2.0,         1.0,        -1.95738904,  0.95853169]
    ]))
    p.connect('green_ex', 'filter')
    p.stage(proc.ZeroCrossingDetector('zero_cross'))
    p.connect('filter', 'zero_cross')
    p.sink('zero_cross_out', 'zero_cross', fill_zero_crossings)

    p.stage(proc.PeakDetector('peaks', eps=0.0, maxOpen=3))
    p.connect('filter', 'peaks')
    p.sink('peaks_out', 'peaks', fill_peaks)

    p.build()
    ppg.configure(3, [4])

    in_ear_pipe = proc.Pipeline('InEar')
    in_ear_pipe.source('skin_temp', skin_temp)
    in_ear_pipe.stage(proc.SwitchStage('temp_switch', (30.0, 32.0)))
    in_ear_pipe.connect('skin_temp', 'temp_switch')
    in_ear_pipe.sink('in_ear_sink', 'temp_switch', detect_inear)

    in_ear_pipe.build()
    skin_temp.configure(1, [4])
