#!/usr/bin/env python3
"""Send one mtk.webui JSON-RPC over the forwarded webui websocket and print the reply."""
import asyncio
import json
import sys

import websockets

URL = "ws://127.0.0.1:17681/mtk.webui.tvCmd?name=FactoryMode"


async def main():
    method = sys.argv[1]
    params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
    out = []
    async with websockets.connect(URL, open_timeout=8, max_size=2**22) as ws:
        await ws.send(json.dumps({"id": 1, "method": method, "params": params}))
        try:
            while True:
                raw = await asyncio.wait_for(ws.recv(), timeout=6)
                out.append(raw)
                obj = json.loads(raw) if raw.strip().startswith("{") else {}
                if obj.get("id") == 1 or "error" in obj:
                    break
        except asyncio.TimeoutError:
            pass
    for line in out:
        print(line)


asyncio.run(main())
