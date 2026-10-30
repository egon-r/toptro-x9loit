#!/usr/bin/env python3
"""One-shot orchestrator: tap into mboot, then hand control back with a live prompt loop.
Run in foreground; prints status with timestamps so the human never guesses."""
import os, serial, time, sys, re
PORT = os.environ.get('X9_TTY', '/dev/ttyUSB0')

PORT='PORT'
LOG=os.environ.get('X9_LOG','orchestrator.log')
log=open(LOG,'ab')

def ts(): return time.strftime('%H:%M:%S')
def say(msg):
    line=f'[{ts()}] {msg}'
    print(line, flush=True); log.write((line+'\n').encode()); log.flush()

s=serial.Serial(PORT,115200,timeout=0.1)
s.setDTR(False); s.setRTS(False)
say('PORT OPEN — settle 3s across ESP32 reset')
time.sleep(3); s.reset_input_buffer()
# deterministic bridge entry: toggle 0x1D until the firmware confirms '# bridge resumed'
for attempt in range(4):
    s.write(b'\x1d'); time.sleep(0.6)
    resp=s.read(4096)
    if b'bridge resumed' in resp:
        say('ESP32 CONFIRMED: transparent bridge active'); break
    if b'prompt (Ctrl-]' in resp:
        say(f'toggle {attempt}: was prompt, toggled again'); continue
    say(f'toggle {attempt}: resp={resp[:60]!r}')
else:
    say('WARNING: bridge state unconfirmed')

say('ARMED FOREVER: tapping continues until the prompt is caught — power on whenever ready.')
buf=b''; prompt=None; taps=0; t0=time.time()
while not prompt:
    d=s.read(4096)
    if d:
        buf+=d; log.write(d); log.flush()
        if b'UART_115200' in buf and len(buf)<4000: say('BOOT BANNER SEEN — ROM is alive')
    if not prompt:
        for _ in range(6):
            s.write(b'\r'); s.flush(); time.sleep(0.016)
        taps+=6
        if b'>>#' in buf[-400:] or b'sh-3.2#' in buf[-60:] or b'sh-3.2$' in buf[-60:]:
            prompt=round(time.time()-t0,1)
            say(f'PROMPT SECURED at t+{prompt}s after {taps} taps')
        if b'Power Down' in buf[-600:]:
            say('MBOOT POWERED DOWN (secondary mode) — flip mains to retry'); buf=b''
        time.sleep(0.12)


say('WATCHDOG MODE: continuous serial log, auto-catch on reboot, interactive at prompt. quit = exit.')
import threading
cmdq=[]
def reader():
    for line in sys.stdin:
        cmdq.append(line.strip())
        if line.strip()=='quit': break
threading.Thread(target=reader,daemon=True).start()
at_prompt=True
while True:
    d=s.read(4096)
    if d:
        buf+=d; log.write(d); log.flush()
        if b'UART_115200' in d: say('RESET DETECTED — boot banner, resuming tap'); at_prompt=False
        if b'>>#' in d[-200:] or b'sh-3.2#' in d[-60:] or b'sh-3.2$' in d[-60:]: at_prompt=True
        if b'Power Down' in d[-300:]: say('MBOOT POWERED DOWN — waiting for mains')
    if not at_prompt:
        for _ in range(6):
            s.write(b'\r'); s.flush(); time.sleep(0.016)
        if b'>>#' in buf[-300:] or b'sh-3.2#' in buf[-60:]: at_prompt=True; say('PROMPT RE-SECURED (auto)')
    while cmdq:
        c=cmdq.pop(0)
        if not c or c=='quit': continue
        s.write((c+'\n').encode()); time.sleep(0.3)
        out=b''; te=time.time()+12
        while time.time()<te:
            d=s.read(4096)
            if not d: continue
            out+=d; buf+=d; log.write(d); log.flush(); te=time.time()+3
            if b'UART_115200' in d:
                say('RESET DETECTED (in-cmd) — breaking out to tap scheduler NOW')
                at_prompt=False; break
            if b'>>#' in d[-200:] or b'sh-3.2#' in d[-60:] or b'sh-3.2$' in d[-60:]: at_prompt=True
        sys.stdout.write(out.decode('utf-8','replace')); sys.stdout.flush()
    time.sleep(0.1)
