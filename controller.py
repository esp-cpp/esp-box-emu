import argparse
import hid
import socket
import time

parser = argparse.ArgumentParser(description='Receive gamepad input and send it via UDP socket to ESP BOX EMU.')
parser.add_argument('--ip', type=str, default="192.168.1.34",
                    help='IPv4 address of the ESP BOX EMU')
parser.add_argument('--port', type=int, default=5000,
                    help='Port for the listener socket on ESP BOX EMU')
args = parser.parse_args()

ip_address = args.ip
port = args.port

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

device_names = ["DUALSHOCK", "Controller"]
controller_device = None

for device in hid.enumerate():
    name = device['product_string']
    vendor_id = device['vendor_id']
    product_id = device['product_id']
    if any(map(name.__contains__, device_names)):
        controller_device = device
        print("Selecting controller:")
        print(f"0x{vendor_id:04x}:0x{product_id:04x} {name}")
        break

if controller_device is None:
    print("Could not find controller!")
    exit(-1)

gamepad = hid.device()
gamepad.open(controller_device['vendor_id'], controller_device['product_id'])
gamepad.set_nonblocking(True)

while True:
    report = gamepad.read(64)
    # report is list of form:
    # [ id,
    #   left_analog_x, left_analog_y,
    #   right_analog_x, right_analog_y,
    #   btn_code1, btn_code2,
    #   unknown,
    #   left_analog_trigger,
    #   right_analog_trigger
    #   ]
    if report:
        # send the report out via socket
        sock.sendto(bytes(report), (ip_address, port))
        # print([f"0x{x:08b}" for x in report])
    time.sleep(0.05)
