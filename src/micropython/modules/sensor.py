import _openearable as oe
import parse_info as pi
from sensor_value import SensorValue, SensorValueGroup, SensorValueComponent

class Sensor:
    def __init__(self, name: str, sensor_id: int, scheme: pi.SensorScheme):
        self.name = name
        self.sensor_id = sensor_id
        self.scheme = scheme
        self._on_data_received_cb = None
        self._unit_lookup = self._build_unit_lookup()

    def _build_unit_lookup(self):
        # Creates a flat map: {"group_name|component_name": "unit"}
        lookup = {}
        for group in self.scheme.groups:
            for comp in group.components:
                lookup[f"{group.name}|{comp.name}"] = comp.unit
        return lookup

    def configure(self, sample_rate_index: int, storage_options: list[pi.SensorConfigOptionsType], completion_handler: callable = None):        
        if not all(option in self.scheme.config_options.available_options for option in storage_options):
            raise ValueError("Invalid storage options provided")
        if self.scheme.config_options.frequency_options is not None:
            if not sample_rate_index < len(self.scheme.config_options.frequency_options.frequencies):
                raise ValueError("Invalid frequency index, max index is " + str(len(self.scheme.config_options.frequency_options.frequencies) - 1))
            if self.scheme.config_options.frequency_options.max_ble_frequency_index is not None and pi.SensorConfigOptionsType.DATA_STREAMING in storage_options:
                if sample_rate_index > self.scheme.config_options.frequency_options.max_ble_frequency_index:
                    raise ValueError("Invalid frequency index, exceeds max BLE frequency index of " + str(self.scheme.config_options.frequency_options.max_ble_frequency_index))
        if completion_handler is not None:
            if not callable(completion_handler):
                raise ValueError("Completion handler must be a callable function")
            # Register the callback for data reception
            self.on_data_received(completion_handler)
        storage_options_mask = 0
        for option in storage_options:
            storage_options_mask |= option

        return oe.config_sensor(self.sensor_id, sample_rate_index, storage_options_mask)

    def on_data_received(self, completion_handler: callable):
        """
        Register a callback that will be called when new sensor data is received.
        The callback should accept a single argument, which is the SensorValue object.
        """
        self._on_data_received_cb = completion_handler
        oe.on_data_received(self.sensor_id, self._handle_data_received)

    def _handle_data_received(self, data_dict):
        cb = self._on_data_received_cb
        if cb:
            sensor_value = self._parse_data(data_dict)
            cb(sensor_value)

    def cancel_data_received(self):
        """
        Unregister the callback for data reception.
        """
        # TODO: implement this method to cancel data reception
        self._on_data_received_cb = None

    def get_config(self) -> tuple[int, int]:
        # TODO: Implement getting the current configuration
        raise NotImplementedError("get_config is not implemented yet")

    def on_config_changed(self, completion_handler: callable):
        """
        Register a callback that will be called when the sensor configuration changes.
        The callback should accept a single argument, which is the new configuration.
        """
        # TODO: implement this method to handle configuration changes
        self._on_config_changed_cb = completion_handler

    def _parse_data(self, data_dict):
        """
        Parse the raw data received from the sensor and return a SensorValue object.
        This method should be called internally when data is received.
        """
        if not isinstance(data_dict, dict):
            raise ValueError("Data must be a dictionary")
        
        timestamp = data_dict.get("timestamp", 0)
        groups = []
        
        for group_name, group_data in data_dict.get("groups", {}).items():
            components = []
            for comp_name, comp_value in group_data.items():
                unit = self._unit_lookup.get(f"{group_name}|{comp_name}", "")
                components.append(SensorValueComponent(comp_name, comp_value, unit))
            groups.append(SensorValueGroup(group_name, components))

        return SensorValue(self.name, timestamp, groups)

    def to_dict(self):
        return {
            "name": self.name,
            "sensor_id": self.sensor_id,
            "scheme": self.scheme
        }
    
    def __repr__(self):
        return "Sensor(name=" + str(self.name) + ", sensor_id=" + str(self.sensor_id) + ", scheme=" + str(self.scheme) + ")"
    
def init_sensors():
    schemes = pi.get_sensor_schemes()
    global sensors
    sensors: dict[int, Sensor] = {}

    for scheme in schemes:
        sensor = Sensor(scheme.name, scheme.id, scheme)
        sensors[sensor.sensor_id] = sensor

    return sensors

def get_sensor(sensor_id: int) -> Sensor:
    if sensor_id in sensors:
        return sensors[sensor_id]
    else:
        raise ValueError(f"Sensor with ID {sensor_id} not found.")
    
def get_sensors() -> list[Sensor]:
    return list(sensors.values())