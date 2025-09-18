import openearable as oe
import processing as proc
from processing import Source, Sink
import time

import app_core

app_info = app_core.AppInfo("Heart Rate App", "A simple heart rate app using PPG data", app_core.AppType.DAEMON)

class Peak:
    def __init__(self, timestamp: int, sign: int, prominence: float):
        self.timestamp = timestamp
        self.sign = sign
        self.prominence = prominence

    def __repr__(self):
        return "Peak(ts={}, sign={}, prom={})".format(self.timestamp, self.sign, self.prominence)

class ZeroCrossing:
    def __init__(self, timestamp: int, value: int):
        self.timestamp = timestamp
        self.value = value

    def __repr__(self):
        return "ZeroCrossing(ts={}, value={})".format(self.timestamp, self.value)

zero_crossings: list[ZeroCrossing] = []
peaks: list[Peak] = []

is_in_ear = False

def reset(timestamp: int):
    global zero_crossings, peaks
    zero_crossings = [zc for zc in zero_crossings if zc.timestamp >= timestamp]
    peaks = [p for p in peaks if p.timestamp >= timestamp]

def fill_zero_crossings(sensor_value: oe.SensorValue):
    if not is_in_ear:
        return
    global zero_crossings
    zero_crossings.append(ZeroCrossing(sensor_value.timestamp, sensor_value.groups[0].components[0].value))
    
def fill_peaks(sensor_value: oe.SensorValue):
    if not is_in_ear:
        return
    global peaks
    peaks.append(Peak(sensor_value.timestamp, sensor_value.groups[0].components[1].value, sensor_value.groups[0].components[2].value))

def detect_inear(sensor_value: oe.SensorValue):
    global is_in_ear
    is_in_ear = sensor_value.groups[0].components[1].value > 0
    if is_in_ear:
        print("In-ear detected")
        reset(sensor_value.timestamp)
    else:
        print("Not in-ear")

class EventType:
    POS_PEAK = 0
    NEG_PEAK = 1
    POS_ZERO_CROSS = 2
    NEG_ZERO_CROSS = 3

def get_event_type(event):
    if isinstance(event, Peak):
        return EventType.POS_PEAK if event.sign > 0 else EventType.NEG_PEAK
    elif isinstance(event, ZeroCrossing):
        return EventType.POS_ZERO_CROSS if event.value > 0 else EventType.NEG_ZERO_CROSS
    else:
        raise ValueError("Unknown event type")

def calc_hr(peaks: list[Peak], zero_crossings: list[ZeroCrossing]):
    # merge peaks and zero crossings into one timeline
    if len(peaks) < 2 or len(zero_crossings) < 2:
        return

    events = peaks + zero_crossings
    events.sort(key=lambda e: e.timestamp)

    # filter out events that are not part of a peak-to-peak cycle
    filtered_events = []
    last_event_type = None
    for event in events:
        event_type = get_event_type(event)
        if last_event_type is None:
            if event_type in (EventType.POS_PEAK, EventType.NEG_PEAK):
                filtered_events.append(event)
                last_event_type = event_type
        else:
            if (last_event_type == EventType.POS_PEAK and event_type == EventType.NEG_ZERO_CROSS) or \
            (last_event_type == EventType.NEG_PEAK and event_type == EventType.POS_ZERO_CROSS):
                filtered_events.append(event)
                last_event_type = event_type
            elif (last_event_type == EventType.NEG_ZERO_CROSS and event_type == EventType.NEG_PEAK) or \
                (last_event_type == EventType.POS_ZERO_CROSS and event_type == EventType.POS_PEAK):
                filtered_events.append(event)
                last_event_type = event_type

    print("All events:", events)
    print("Filtered events:", filtered_events)

    # calculate the avg timedifference between the negative zero crossings
    negative_crossings_timestamps = [e.timestamp for e in filtered_events if isinstance(e, ZeroCrossing) and e.value < 0]

    if len(negative_crossings_timestamps) < 2:
        return

    avg_dt = sum(negative_crossings_timestamps[i] - negative_crossings_timestamps[i - 1] for i in range(1, len(negative_crossings_timestamps))) / (len(negative_crossings_timestamps) - 1)
    bpm = 60.0 / (avg_dt / 1000000.0)  # convert µs to s

    print("Estimated BPM: {:.2f}".format(bpm))

    reset(filtered_events[-1].timestamp)

def main():
    print("Starting Heart Rate App")

    oe.init_sensors()
    ppg = oe.get_sensor(4)
    skin_temp = [s for s in oe.get_sensors() if s.name == "Skin Temperature Sensor"][0]

    p = proc.Pipeline('HR')
    p.source('ppg', ppg)
    p.stage(proc.ComponentExtractor('green_ex', group='PHOTOPLETHYSMOGRAPHY', component='GREEN'))
    p.connect('ppg', 'green_ex')
    p.stage(proc.BiQuadFilter('filter', stages=2, coeffs=[[ 0.01658193,  0.03316386,  0.01658193,         -1.62885077,  0.69946348], [ 1.,         -2.,          1.,                   -1.95738904,  0.95853169]]))
    p.connect('green_ex', 'filter')
    p.stage(proc.ZeroCrossingDetector('zero_cross'))
    p.connect('filter', 'zero_cross')
    p.sink('zero_cross_out', 'zero_cross', fill_zero_crossings)
    p.stage(proc.PeakDetector('peaks', eps=0.0, maxOpen=3))
    p.connect('filter', 'peaks')
    p.sink('accel_peak_out', 'peaks', fill_peaks)

    p.build()
    ppg.configure(3, [4])

    in_ear_pipe = proc.Pipeline('InEar')
    in_ear_pipe.source('skin_temp', skin_temp)
    in_ear_pipe.stage(proc.SwitchStage('temp_switch', (30.0, 32.0)))
    in_ear_pipe.connect('skin_temp', 'temp_switch')
    in_ear_pipe.sink('in_ear_sink', 'temp_switch', detect_inear)

    in_ear_pipe.build()
    skin_temp.configure(1, [4])

    while True:
        # check if lists have enough data
        if len(peaks) >= 10 and len(zero_crossings) >= 10:
            calc_hr(peaks, zero_crossings)
        time.sleep(1)