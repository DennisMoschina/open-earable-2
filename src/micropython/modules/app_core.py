class AppType:
    """
    Enum-like class to represent different types of applications.
    """
    APP = 0
    DAEMON = 1

class AppStatus:
    """
    Enum-like class to represent the status of an application.
    """
    READY = 0
    RUNNING = 1
    PAUSED = 2
    STOPPED = 3
    ERROR = 4

class AppInfo:
    """
    Class to hold information about the application.
    """
    def __init__(self, name: str, description: str, type: AppType, status: AppStatus = AppStatus.READY):
        """
        Initialize the application information.
        """
        self.name = name
        self.description = description
        self.type = type
        self.status = status