import _openearable as oe

# ParseType Enum Equivalent
class ParseType:
    INT8 = 0
    UINT8 = 1
    INT16 = 2
    UINT16 = 3
    INT32 = 4
    UINT32 = 5
    FLOAT = 6
    DOUBLE = 7


# SensorComponent class
class SensorComponent:
    def __init__(self, name: str, unit: str, parse_type: ParseType):
        self.name = name
        self.unit = unit
        self.parse_type = parse_type
    
    def __repr__(self):
        return "SensorComponent(name=" + str(self.name) + ", unit=" + str(self.unit) + ", parse_type=" + str(self.parse_type) + ")"


# SensorComponentGroup class
class SensorComponentGroup:
    def __init__(self, name: str, components: list[SensorComponent] = []):
        self.name = name
        self.components = components

    def __repr__(self):
        return "SensorComponentGroup(name=" + str(self.name) + ", components=" + str(self.components) + ")"


# SensorConfigOptionsMasks (bitmask constants)
class SensorConfigOptionsType:
    DATA_STREAMING = 0x01
    DATA_STORAGE = 0x02
    FREQUENCIES_DEFINED = 0x10


# FrequencyOptions class
class FrequencyOptions:
    def __init__(self, frequencies: list[float], default_index: int = 0, max_ble_index: int = None):
        self.frequencies = frequencies
        self.default_frequency_index = default_index
        self.max_ble_frequency_index = max_ble_index

    def __repr__(self):
        return "FrequencyOptions(frequencies=" + str(self.frequencies) + ", default_index=" + str(self.default_frequency_index) + ", max_ble_index=" + str(self.max_ble_frequency_index) + ")"

# SensorConfigOptions class
class SensorConfigOptions:
    def __init__(self, available_options: list[SensorConfigOptionsType], frequency_options: FrequencyOptions = None):
        self.available_options = available_options
        self.frequency_options = frequency_options

    def __repr__(self):
        return "SensorConfigOptions(available_options=" + str(self.available_options) + ", frequency_options=" + str(self.frequency_options) + ")"
    

# SensorScheme class
class SensorScheme:
    def __init__(self, name: str, sensor_id: int, groups: list[SensorComponentGroup] = [], config_options: SensorConfigOptions = None):
        self.name = name
        self.id = sensor_id
        self.groups = groups
        self.config_options = config_options

    def __repr__(self):
        return "SensorScheme(name=" + str(self.name) + ", id=" + str(self.id) + ", groups=" + str(self.groups) + ", config_options=" + str(self.config_options) + ")"

def get_sensor_schemes():
    raw_data = oe.get_sensor_schemes()  # Calls the C function returning list of tuples
    sensor_schemes = []

    for name, sensor_id, group_tuples, options in raw_data:
        groups = []

        for group_name, component_tuples in group_tuples:
            components = []
            for comp_name, unit, parse_type in component_tuples:
                component = SensorComponent(comp_name, unit, parse_type)
                components.append(component)

            group = SensorComponentGroup(group_name, components)
            groups.append(group)

        raw_option_types, raw_freq_options = options
        option_types = []

        if SensorConfigOptionsType.DATA_STORAGE & raw_option_types:
            option_types.append(SensorConfigOptionsType.DATA_STORAGE)
        if SensorConfigOptionsType.DATA_STREAMING & raw_option_types:
            option_types.append(SensorConfigOptionsType.DATA_STREAMING)
        if SensorConfigOptionsType.FREQUENCIES_DEFINED & raw_option_types:
            option_types.append(SensorConfigOptionsType.FREQUENCIES_DEFINED)

        frequencies, default_freq, max_ble_freq = raw_freq_options
        freq_options = FrequencyOptions(frequencies, default_freq, max_ble_freq)

        config_options = SensorConfigOptions(option_types, freq_options)

        scheme = SensorScheme(name, sensor_id, groups, config_options)
        sensor_schemes.append(scheme)

    return sensor_schemes