import bluetooth
from micropython import const
import os
import sys
import app_core
from time import sleep

APPS_DIR = 'apps'

_LAUNCHER_SERVICE_UUID = bluetooth.UUID("12345678-1234-1234-1234-1234567890ab")
_LIST_APPS_CHAR_UUID = bluetooth.UUID("12345678-1234-1234-1234-000000000001")
_REQUEST_APP_INFO_CHAR_UUID = bluetooth.UUID("12345678-1234-1234-1234-000000000002")
_APP_INFO_CHAR_UUID = bluetooth.UUID("12345678-1234-1234-1234-000000000003")
_START_APP_CHAR_UUID = bluetooth.UUID("12345678-1234-1234-1234-000000000004")

_IRQ_CENTRAL_CONNECT = const(1)
_IRQ_CENTRAL_DISCONNECT = const(2)
_IRQ_GATTS_WRITE = const(3)

ble = bluetooth.BLE()

connections = set()
list_apps_handle = None
request_app_info_handle = None
app_info_handle = None
start_app_handle = None

apps = {}

class AppException(Exception):
    """Custom exception for app-related errors."""
    def __init__(self, message):
        super().__init__(message)

def _irg(event, data):
    if event == _IRQ_CENTRAL_CONNECT:
        conn_handle, _, _ = data
        print("Connected:", conn_handle)
        connections.add(conn_handle)
        start()

    elif event == _IRQ_CENTRAL_DISCONNECT:
        conn_handle, _, _ = data
        print("Disconnected:", conn_handle)
        connections.discard(conn_handle)

    elif event == _IRQ_GATTS_WRITE:
        conn_handle, attr_handle = data

        if attr_handle == request_app_info_handle:
            raw = ble.gatts_read(request_app_info_handle)
            app_name = raw.decode('utf-8').strip()

            info = get_app_info(app_name)
            if info:
                info_str = '{"name":"' + info.name + '","description":"' + info.description + '","type":' + str(info.type) + ',"status":' + str(info.status) + '}'
                ble.gatts_write(app_info_handle, info_str.encode('utf-8'))
                print("Sent app info for:", app_name)
            else:
                print("App info not found for:", app_name)

        elif attr_handle == start_app_handle:
            raw = ble.gatts_read(start_app_handle)
            app_name = raw.decode('utf-8').strip()

            try:
                print("Starting app:", app_name)
                start_app(app_name)
            except Exception as e:
                print("Failed to start app:", app_name, str(e))

ble.irq(_irg)

SERVICE = (
    _LAUNCHER_SERVICE_UUID,
    (
        (_LIST_APPS_CHAR_UUID, bluetooth.FLAG_READ),
        (_REQUEST_APP_INFO_CHAR_UUID, bluetooth.FLAG_WRITE),
        (_APP_INFO_CHAR_UUID, bluetooth.FLAG_READ | bluetooth.FLAG_NOTIFY),
        (_START_APP_CHAR_UUID, bluetooth.FLAG_WRITE),
    ),
)

def list_apps():
    app_names = []
    for file in os.listdir(APPS_DIR):
        if file.endswith('.py'):
            app_names.append(file[:-3])  # remove .py extension
    return app_names

def load_app(app_name):
    if APPS_DIR not in sys.path:
        sys.path.append(APPS_DIR)
    try:
        module = __import__(app_name)
        if not hasattr(module, 'main') or not callable(module.main) or not hasattr(module, 'app_info'):
            raise AppException("App " + app_name + " does not have a valid main function or app_info.")
        return module
    except AppException as e:
        print("Error loading app " + app_name + ": " + str(e))
        return None

def get_app_info(app_name):
    if app_name in apps:
        return apps[app_name].app_info
    else:
        app_module = load_app(app_name)
        if app_module:
            apps[app_name] = app_module
            return app_module.app_info
        else:
            return None

def start_app(app_name):
    if app_name in apps:
        app_module = apps[app_name]
    else:
        app_module = load_app(app_name)
        if not app_module:
            raise AppException("App " + app_name + " could not be loaded.")
    app_module.main()

def _set_app_names(app_names):
    if not app_names:
        ble.gatts_write(list_apps_handle, b'')
    else:
        ble.gatts_write(list_apps_handle, ','.join(app_names).encode('utf-8'))

def init():
    sleep(1)
    ble.active(True)

    if not ble.active():
        raise AppException("Bluetooth is not active. Cannot initialize app launcher.")
    else:
        print("Bluetooth is active. Initializing app launcher...")
        start()

def start():
    global list_apps_handle, request_app_info_handle, app_info_handle, start_app_handle

    print("Starting App Launcher Service")

    handles = ble.gatts_register_services((SERVICE,))
    list_apps_handle, request_app_info_handle, app_info_handle, start_app_handle = handles[0]

    app_names = list_apps()
    _set_app_names(app_names)

    for app_name in app_names:
        app_module = load_app(app_name)
        if app_module:
            apps[app_name] = app_module
            print("Loaded app: " + app_name)
            if app_module.app_info.type == app_core.AppType.DAEMON:
                print("Starting daemon app: " + app_name)
                app_module.main()

init()