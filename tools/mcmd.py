#!/usr/bin/env python3
import os, serial, sys, time
PORT = os.environ.get('X9_TTY', '/dev/ttyUSB0')
s = serial.Serial('PORT', 115200, timeout=0.2)
s.setDTR(False); s.setRTS(False)
time.sleep(3.0)                 # survive the open-time ESP32 reset
s.reset_input_buffer()
for c in sys.argv[1].split(';'):
        s.write((c.strip()+'\n').encode())
        time.sleep(1.0)
out=b''
s.timeout=0.4
t0=time.time()
while time.time()-t0 < 40:
    d=s.read(65536)
    if d:
        out+=d; t0=time.time()
        sys.stdout.buffer.write(d); sys.stdout.buffer.flush()   # LIVE stream
        sys.stdout.flush()

s.close()
sys.stdout.buffer.write(out)
