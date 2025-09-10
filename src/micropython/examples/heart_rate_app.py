import openearable as oe
import processing as proc
from processing import Source, Sink

oe.init_sensors()
ppg = oe.get_sensor(4)

zero_crossings: list[tuple[int, int]] = []

def calc_hr(sensor_value: oe.SensorValue):
    zero_crossings.append((sensor_value.timestamp, sensor_value.groups[0].components[0].value))
    if len(zero_crossings) < 10:
        return
    
    # calculate the avg timedifference between the positive zero crossings
    positive_crossings = [ts for ts, val in zero_crossings if val == 1]
    if len(positive_crossings) < 2:
        return

    avg_dt = sum(positive_crossings[i] - positive_crossings[i - 1] for i in range(1, len(positive_crossings))) / (len(positive_crossings) - 1)
    bpm = 60.0 / (avg_dt / 1000000.0)  # convert µs to s

    print("Estimated BPM: {:.2f}".format(bpm))

    zero_crossings.clear()

p = proc.Pipeline('HR')
p.source('ppg', ppg)
p.stage(proc.ComponentExtractor('green_ex', group='PHOTOPLETHYSMOGRAPHY', component='GREEN'))
p.connect('ppg', 'green_ex')
p.stage(proc.BiQuadFilter('filter', stages=2, coeffs=[[ 0.01658193,  0.03316386,  0.01658193,         -1.62885077,  0.69946348], [ 1.,         -2.,          1.,                   -1.95738904,  0.95853169]]))
# p.stage(proc.BiQuadFilter('filter', stages=1, coeffs=[[0.32512399, 0, -0.32512399, -1.18279670, 0.34975201]]))
# p.stage(proc.BiQuadFilter('filter', stages=1, coeffs=[[0.05908081, 0, -0.05908081, -1.87812501, 0.88183839]])) # sample rate index 4
p.connect('green_ex', 'filter')
p.stage(proc.ZeroCrossingDetector('zero_cross'))
p.connect('filter', 'zero_cross')
p.sink('zero_cross_out', 'zero_cross', calc_hr)

p.build()
ppg.configure(3, [4])

