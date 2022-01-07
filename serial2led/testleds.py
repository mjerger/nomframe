import serial, sys, random

serial = serial.Serial()
serial.baudrate = 1000000
serial.port = sys.argv[1]
serial.open()

for f in range(0,100):
    serial.read()
    r = 255 if f%2 else 0
    for i in range(0, 256):
        serial.write([r,r,r])
    serial.flush()