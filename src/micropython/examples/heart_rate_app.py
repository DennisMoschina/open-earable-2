import openearable as oe
import processing as proc
from processing import Source, Sink
import time
import app_core

# ==== BLE (functional style like your App Launcher) ====
try:
    import bluetooth
    from micropython import const
    import struct
except ImportError:
    bluetooth = None  # If BLE isn't available in this firmware

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

# ==== Globals to feed BLE HRS ====
_last_bpm_int = None
_last_ibi_us = None

# ==== HRS (functional) ====
# UUIDs
_UUID_HRS = 0x180D
_UUID_HRM = 0x2A37
_UUID_BSL = 0x2A38

# IRQ constants
_IRQ_CENTRAL_CONNECT = const(1)
_IRQ_CENTRAL_DISCONNECT = const(2)
_IRQ_GATTS_WRITE = const(3)

# Handles / state
ble = None
_hrs_enabled = False
_connections = set()
_hrm_handle = None
_bsl_handle = None
_adv_payload = None
_device_name = "OpenEarable HR"
_body_sensor_location = 0x06  # "Other" (no specific "ear" code in spec)

def _adv_payload_make(name=None, services_16=None):
    if name is None and not services_16:
        return b"\x02\x01\x06"  # flags only
    payload = bytearray()

    def _append(adv_type, value_bytes):
        payload.extend((len(value_bytes) + 1, adv_type))
        payload.extend(value_bytes)

    if name:
        _append(0x09, name.encode())  # Complete Local Name

    if services_16:
        # Complete List of 16-bit Service Class UUIDs
        sv = bytearray()
        for u16 in services_16:
            sv.extend(struct.pack("<H", u16))
        if sv:
            _append(0x03, sv)

    # Could also add appearance if desired (Generic Heart Rate Sensor = 0x0341)
    return bytes(payload)

def _hrs_irq(event, data):
    global _connections
    if event == _IRQ_CENTRAL_CONNECT:
        conn_handle, _, _ = data
        _connections.add(conn_handle)
        # Advertising typically stops automatically on connect
    elif event == _IRQ_CENTRAL_DISCONNECT:
        conn_handle, _, _ = data
        _connections.discard(conn_handle)
        _hrs_advertise()  # resume advertising on disconnect
    # No writable chars here—no _IRQ_GATTS_WRITE handling needed

def _hrs_register():
    # Register GATT service and chars
    global _hrm_handle, _bsl_handle
    SERVICE = (
        bluetooth.UUID(_UUID_HRS),
        (
            # Heart Rate Measurement: Notify + Read (read mirrors last notified value)
            (bluetooth.UUID(_UUID_HRM), bluetooth.FLAG_NOTIFY | bluetooth.FLAG_READ),
            # Body Sensor Location: Read (0x06 = other)
            (bluetooth.UUID(_UUID_BSL), bluetooth.FLAG_READ),
        ),
    )
    (( _hrm_handle, _bsl_handle),) = ble.gatts_register_services((SERVICE,))
    # Set BSL value
    ble.gatts_write(_bsl_handle, bytes([_body_sensor_location]))

def _hrs_advertise(interval_us=250_000):
    # (Re)start advertising the HRS with name
    global _adv_payload
    if _adv_payload is None:
        _adv_payload = _adv_payload_make(name=_device_name, services_16=[_UUID_HRS])
    try:
        ble.gap_advertise(interval_us, adv_data=_adv_payload)
    except Exception as e:
        # Keep app running even if advertising fails once
        print("HRS advertise error:", e)

def hrs_init(device_name="OpenEarable HR", body_sensor_location=0x06):
    """Initialize BLE and the Heart Rate Service (functional style)."""
    global ble, _hrs_enabled, _device_name, _body_sensor_location, _connections, _adv_payload
    if bluetooth is None:
        print("BLE not available in this firmware.")
        _hrs_enabled = False
        return

    _device_name = device_name
    _body_sensor_location = body_sensor_location
    _connections = set()
    _adv_payload = None

    try:
        ble = bluetooth.BLE()
        ble.active(True)
        ble.irq(_hrs_irq)

        _hrs_register()
        _hrs_advertise()
        _hrs_enabled = True
        print("HRS initialized (UUID 0x180D); advertising…")
    except Exception as e:
        print("HRS init failed:", e)
        _hrs_enabled = False

def hrs_is_ready():
    return _hrs_enabled and ble is not None and _hrm_handle is not None

def hrs_notify(bpm_int, ibi_us=None, contact_detected=False):
    """Send Heart Rate Measurement notifications to all connected centrals."""
    if not hrs_is_ready() or not _connections:
        return

    # Flags:
    # bit0: 0 = HR uint8, 1 = HR uint16
    # bit1: Sensor Contact feature supported (1)
    # bit2: Sensor Contact detected (depends on in-ear)
    # bit3: Energy Expended present (0 here)
    # bit4: RR-Interval present (1 if ibi provided)
    flags = 0x02  # contact supported
    if contact_detected:
        flags |= 0x04
    rr_present = ibi_us is not None
    if rr_present:
        flags |= 0x10

    if bpm_int <= 255:
        buf = bytearray(2)
        buf[0] = flags
        buf[1] = bpm_int & 0xFF
    else:
        flags |= 0x01  # 16-bit HR format
        buf = bytearray(3)
        buf[0] = flags
        buf[1:3] = struct.pack("<H", bpm_int & 0xFFFF)

    # Append one RR-interval (in 1/1024 s units) if provided
    if rr_present:
        rr_1_1024 = int((ibi_us * 1024 + 500_000) // 1_000_000)
        if rr_1_1024 < 1:
            rr_1_1024 = 1
        if rr_1_1024 > 0xFFFF:
            rr_1_1024 = 0xFFFF
        buf += struct.pack("<H", rr_1_1024)

    # Mirror value to make it readable as well (optional but handy)
    try:
        ble.gatts_write(_hrm_handle, bytes(buf))
    except Exception as e:
        print("HRS gatts_write error:", e)

    # Notify all active connections
    for ch in tuple(_connections):
        try:
            ble.gatts_notify(ch, _hrm_handle, bytes(buf))
        except Exception as e:
            print("HRS gatts_notify error:", e)

# =======================
# Small helpers (no imports)
# =======================
def _insert_sorted(buf, val, maxlen):
    n = len(buf)
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
        med = _median(buf)
        if abs(buf[0] - med) > abs(buf[-1] - med):
            buf.pop(0)
        else:
            buf.pop()

def _median(buf):
    n = len(buf)
    if n == 0:
        return None
    m = n >> 1
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
# Sinks (unchanged HR logic; only call hrs_notify)
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
    _insert_sorted(_prom_sorted, prom, PROM_WIN)

    med_prom = _median(_prom_sorted)
    if (med_prom is None) or (prom >= _PROM_FRAC * med_prom):
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

    global _last_neg_zc_ts, _had_good_peak_since_last_negzc, _last_bpm_int, _last_ibi_us

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
        return

    # Missed-beat & short-duplicate handling
    if dt > _MISSED_FACTOR * med and (dt >> 1) >= _MIN_PERIOD_US:
        half = dt / 2.0
        _insert_sorted(_ibi_sorted, half, IBI_WIN)
        _insert_sorted(_ibi_sorted, half, IBI_WIN)
        _last_ibi_us = int(half)
    elif dt < _SHORT_FACTOR * med:
        # likely duplicate / ringing → drop
        _last_ibi_us = None
    else:
        _insert_sorted(_ibi_sorted, dt, IBI_WIN)
        _last_ibi_us = int(dt)

    # Compute BPM from robust central tendency:
    med = _median(_ibi_sorted)
    if med is not None:
        bpm = 60.0 * 1_000_000.0 / med
        # print ONLY BPM int
        bpm_int = int(bpm)
        _last_bpm_int = bpm_int
        print("HR: {:3d} bpm".format(bpm_int))

        # Notify BLE HRS (functional)
        hrs_notify(bpm_int=bpm_int, ibi_us=_last_ibi_us, contact_detected=is_in_ear)

    _last_neg_zc_ts = ts
    _last_counted_neg_zc_ts = ts
    _had_good_peak_since_last_negzc = False

# =======================
# Main (setup pipelines, run forever)
# =======================
def main():
    # ---- Init BLE Heart Rate Service (functional) ----
    if bluetooth is not None:
        hrs_init(device_name="OpenEarable HR", body_sensor_location=0x06)
    else:
        print("BLE not available in this firmware.")

    # ---- Pipelines (unchanged math) ----
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
    in_ear_pipe.stage(proc.SwitchStage('temp_switch', (32.0, 34.0)))
    in_ear_pipe.connect('skin_temp', 'temp_switch')
    in_ear_pipe.sink('in_ear_sink', 'temp_switch', detect_inear)

    in_ear_pipe.build()
    skin_temp.configure(1, [4])
