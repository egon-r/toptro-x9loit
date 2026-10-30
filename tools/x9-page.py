#!/usr/bin/env python3
"""Drive the X9 browser page (via x9-cdp-shell.py bridge) for eval / sraf file reads / factory RPC.

Usage:
  x9-page.py eval   '<js expression>'        # page-context eval, prints the value
  x9-page.py read   <file-or-url> [hash]     # sraf fetch, prints base64 length + text
  x9-page.py ls     <file-url>               # sraf fetch, prints raw bytes as text/hex
  x9-page.py rpc    <method> '<json>' [secs] # factory websocket RPC, prints every message
  x9-page.py raw    '<js>'                   # enqueue only, prints the queued id

The page polls the bridge itself; results land in /results as {cmd: tag, response|error}.
"""
import json
import sys
import time
import urllib.request

import os
BASE = os.environ.get("X9_BASE", "http://127.0.0.1:8089")
TAG = "x9p"


def post(path, obj):
    body = json.dumps(obj).encode()
    req = urllib.request.Request(BASE + path, data=body, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=10) as r:
        return r.read()


def get(path):
    with urllib.request.urlopen(BASE + path, timeout=10) as r:
        return r.read()


def results():
    return json.loads(get("/results"))


def enqueue(js, tag):
    post("/cmd", {"js": js, "tag": tag})


def wait_for(tag, timeout=40.0, since=0):
    """Return the response object for `tag`, only among results after index `since`."""
    end = time.time() + timeout
    while time.time() < end:
        for r in reversed(results()[since:]):
            if r.get("cmd") == tag:
                return r
        time.sleep(0.4)
    return None


def js_eval(expr, tag=None, timeout=40.0):
    tag = tag or ("x9p%d" % (int(time.time() * 1000) % 100000000))
    since = len(results())
    enqueue(expr, tag)
    r = wait_for(tag, timeout, since)
    if r is None:
        raise SystemExit(f"timeout waiting for tag {tag}")
    return r


def unwrap(r):
    if "error" in r:
        return {"error": r["error"]}
    inner = r.get("response", {}).get("result", {})
    if "result" in inner:          # CDP shape
        return inner["result"].get("value", inner["result"])
    return inner


# sraf fetch: the page exposes nfetch(hash,url) + window.__jl style event takers.
SRAF_TPL = """
new Promise(function(res,rej){
  var h=%(h)s;
  sraf.addEventTaker(function(e){ if(e.hash===h) res(e); });
  try{ sraf.sendSync(JSON.stringify({type:SRAF_CMD_NET_HTTP_REQUEST,hash:h,url:%(u)s})); }
  catch(err){ rej('sendSync '+err.message); }
  setTimeout(function(){ rej('sraf timeout'); }, %(ms)d);
}).then(function(e){
  var c=e.content||'';
  return JSON.stringify({hash:e.hash, code:(e.code===undefined?null:e.code), len:c.length, b64:btoa(c)});
})
"""


def sraf_fetch(url, timeout_ms=12000):
    h = "x9f%d" % (int(time.time() * 1000) % 1000000)
    expr = SRAF_TPL % {"h": json.dumps(h), "u": json.dumps(url), "ms": timeout_ms}
    r = js_eval(expr, tag="sraf%d" % (int(time.time()) % 100000), timeout=timeout_ms / 1000 + 25)
    v = unwrap(r)
    if isinstance(v, str):
        try:
            return json.loads(v)
        except ValueError:
            return {"raw": v}
    return v


RPC_TPL = """
new Promise(function(res){
  var msgs=[], ws=new WebSocket('ws://127.0.0.1:7681/mtk.webui.tvCmd?name=FactoryMode');
  var done=false;
  function fin(){ if(done)return; done=true; try{ws.close();}catch(e){} res(JSON.stringify(msgs)); }
  ws.onopen=function(){ ws.send(JSON.stringify(%(req)s)); };
  ws.onmessage=function(ev){ msgs.push(ev.data); if(msgs.length>=%(max)d) setTimeout(fin,300); };
  ws.onerror=function(){ msgs.push('__WS_ERROR__'); fin(); };
  ws.onclose=function(){ };
  setTimeout(fin, %(ms)d);
})
"""


def rpc(method, params, seconds=6.0, tag=None):
    req = json.dumps({"id": 1, "method": method, "params": params})
    expr = RPC_TPL % {"req": req, "max": 6, "ms": int(seconds * 1000)}
    tag = tag or ("rpc%d" % (int(time.time() * 1000) % 1000000))
    r = js_eval(expr, tag=tag, timeout=seconds + 25)
    v = unwrap(r)
    try:
        return json.loads(v) if isinstance(v, str) else v
    except ValueError:
        return {"raw": v}


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    cmd = sys.argv[1]
    if cmd == "eval":
        print(json.dumps(unwrap(js_eval(sys.argv[2])), indent=1))
    elif cmd == "raw":
        enqueue(sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else TAG)
        print("queued")
    elif cmd in ("read", "ls"):
        info = sraf_fetch(sys.argv[2])
        if "b64" in info:
            data = __import__("base64").b64decode(info["b64"])
            if cmd == "read":
                print("len=%d code=%s" % (len(data), info.get("code")))
                sys.stdout.write(data.decode("utf-8", "replace"))
            else:
                print("len=%d code=%s hex=%s" % (len(data), info.get("code"), data[:64].hex()))
                sys.stdout.write(data.decode("utf-8", "replace"))
        else:
            print(json.dumps(info, indent=1)[:2000])
    elif cmd == "rpc":
        params = json.loads(sys.argv[3]) if len(sys.argv) > 3 else {}
        secs = float(sys.argv[4]) if len(sys.argv) > 4 else 6.0
        for m in rpc(sys.argv[2], params, secs):
            print(m)
    else:
        raise SystemExit(__doc__)


if __name__ == "__main__":
    main()
