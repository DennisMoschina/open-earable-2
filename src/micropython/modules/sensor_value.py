class SensorValueComponent:
    def __init__(self, name: str, value, unit: str):
        self.name = name
        self.value = value
        self.unit = unit

    def to_dict(self):
        return {
            "name": self.name,
            "value": self.value,
            "unit": self.unit
        }
    
    def __repr__(self):
        return "SensorValueComponent(name=" + str(self.name) + ", value=" + str(self.value) + ", unit=" + str(self.unit) + ")"

class SensorValueGroup:
    def __init__(self, name: str, components: list[SensorValueComponent]):
        self.name = name
        self.components = components

    def to_dict(self):
        return {
            "name": self.name,
            "components": [comp.to_dict() for comp in self.components]
        }
    
    def __repr__(self):
        return "SensorValueGroup(name=" + str(self.name) + ", components=" + str(self.components) + ")"

class SensorValue:
    def __init__(self, name: str, timestamp: int, groups: list[SensorValueGroup]):
        self.name = name
        self.timestamp = timestamp
        self.groups = groups

    def to_dict(self):
        return {
            "name": self.name,
            "timestamp": self.timestamp,
            "groups": [
                {
                    "name": group.name,
                    "components": [
                        {"name": comp.name, "value": comp.value, "unit": comp.unit}
                        for comp in group.components
                    ]
                } for group in self.groups
            ]
        }

    def __repr__(self):
        return "SensorValue(name=" + str(self.name) + ", timestamp=" + str(self.timestamp) + ", groups=" + str(self.groups) + ")"