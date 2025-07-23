import _openearable as oe

class Color:
    """Color class to represent RGB colors."""
    def __init__(self, r, g, b):
        self.r = r
        self.g = g
        self.b = b

    def __repr__(self):
        return f"Color(r={self.r}, g={self.g}, b={self.b})"
    
class LedMode:
    """Enum-like class to represent LED modes."""
    STATE_INDICATION = 0
    CUSTOM_COLOR = 1

def set_led_color(color: Color):
    """Set the LED color."""
    # This function would interface with the hardware to set the LED color.
    # For demonstration, we will just print the color.
    print(f"Setting LED color to: {color}")
    oe.set_led_color(color.r, color.g, color.b)

def set_led_mode(mode: LedMode):
    """Set the LED mode."""
    # This function would interface with the hardware to set the LED mode.
    # For demonstration, we will just print the mode.
    print(f"Setting LED mode to: {mode}")
    oe.set_led_mode(mode)