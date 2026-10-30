import os, serial, time, sys
PORT = os.environ.get('X9_TTY', '/dev/ttyUSB0')
LOG = os.environ.get('X9_LOG', 'serial.log')
s=serial.Serial('PORT',115200,timeout=0.2)
s.setDTR(False); s.setRTS(False)
log=open(LOG,'ab')
time.sleep(3); s.reset_input_buffer()
print('PASSIVE LOGGER — no taps, logging only', flush=True)
while True:
    d=s.read(4096)
    if d:
        log.write(d); log.flush()
        try: sys.stdout.write(d.decode('utf-8','replace')); sys.stdout.flush()
        except Exception: pass
