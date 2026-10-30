#!/usr/bin/env python3
"""Interactive CDP shell host for the X9 browser.

Serves on 0.0.0.0:8089 (override with X9_HOST/X9_PORT):
  GET  /          shell page (reads DevToolsActivePort via sraf file:// fetch,
                  opens ws://127.0.0.1:<port><path>, runs queued CDP commands)
  GET  /queue?n=N pending commands after id N
  POST /cmd       enqueue one command {method, params, js} (js = Runtime.evaluate expr)
  POST /result    page posts each CDP response
  GET  /results   all responses so far
"""
import json, threading, time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

import os
HOST = os.environ.get('X9_HOST', '0.0.0.0'); PORT = int(os.environ.get('X9_PORT', 8089))
OUT = Path(os.environ.get('X9_LOGDIR', 'cdp-session')); OUT.mkdir(parents=True, exist_ok=True)
lock = threading.Lock()
queue, results = [], []

PAGE = '''<!doctype html><meta charset="utf-8"><title>x9 cdp shell</title>
<style>body{background:#11243a;color:#fff;font:18px monospace;white-space:pre-wrap;margin:28px}</style>
<body>shell booting…<script>
(function(){
  var log=[];
  function render(){document.body.textContent=log.join('\\n');}
  function L(s){log.push(s);render();try{var x=new XMLHttpRequest();x.open('POST','/result',true);x.setRequestHeader('Content-Type','application/json');x.send(JSON.stringify({cmd:'log',line:s}));}catch(e){}}
  L('shell start '+new Date().toISOString());
  var dtap=null;
  sraf.addEventTaker(function(e){if(e.hash==='dtap')dtap=e;if(e.hash==='jl')window.__jl=e;});
  function nfetch(hash,url){try{sraf.sendSync(JSON.stringify({type:SRAF_CMD_NET_HTTP_REQUEST,hash:hash,url:url}));}catch(e){L('nfetch err '+e.message);}}
  nfetch('dtap','file:///data/sraf/home/DevToolsActivePort');
  var tries=0;
  var t=setInterval(function(){
    tries++;
    if(!dtap){if(tries>20){L('no DevToolsActivePort event');clearInterval(t);}return;}
    clearInterval(t);
    var txt=atob(dtap.content||'');
    var lines=txt.trim().split('\\n');
    var port=lines[0], path=lines[1];
    L('devtools port '+port+' path '+path);
    var ws=new WebSocket('ws://127.0.0.1:'+port+path);
    var ws2=null, pageUrl=null;
    var nextId=1, pending={}, waiting={}, route={};
    ws.onopen=function(){
      L('browser WS OPEN');
      nfetch('jl','http://127.0.0.1:'+port+'/json/list');
      setTimeout(function(){
        try{
          var list=JSON.parse(atob((window.__jl&&window.__jl.content)||'[]'));
          var me=list.filter(function(t){return t.url.indexOf(location.host)>=0;})[0];
          if(!me){L('no page target in /json/list: '+JSON.stringify(list).slice(0,200));return;}
          pageUrl=me.webSocketDebuggerUrl;
          L('page target '+pageUrl);
          ws2=new WebSocket(pageUrl);
          ws2.onopen=function(){L('page WS OPEN — full Runtime live');};
          ws2.onclose=function(){L('page WS CLOSED');};
          ws2.onmessage=ws.onmessage;
        }catch(e){L('jsonlist err '+e.message);}
      },3000);
    };
    ws.onclose=function(){L('WS CLOSED');};
    setInterval(poll,1500);poll();
    ws.onerror=function(){L('WS ERROR');};
    ws.onmessage=function(ev){
      var m;try{m=JSON.parse(ev.data);}catch(e){return;}
      if(m.id&&pending[m.id]){
        post({cmd:pending[m.id],response:m});
        delete pending[m.id];
        if(waiting[m.id]){clearTimeout(waiting[m.id]);delete waiting[m.id];}
      }
    };
    function send(method,params,tag,sock){
      var s=sock||ws;
      var id=nextId++;pending[id]=tag;
      s.send(JSON.stringify({id:id,method:method,params:params||{}}));
      waiting[id]=setTimeout(function(){post({cmd:tag,error:'timeout'});delete pending[id];delete waiting[id];},25000);
    }
    function post(obj){try{var x=new XMLHttpRequest();x.open('POST','/result',true);x.setRequestHeader('Content-Type','application/json');x.send(JSON.stringify(obj));}catch(e){}}
    function poll(){
      try{
        var x=new XMLHttpRequest();
        x.onload=function(){
          var cmds;try{cmds=JSON.parse(x.responseText);}catch(e){return;}
    function pageeval(code,tag){
      try{
        Promise.resolve(eval(code)).then(function(v){
          post({cmd:tag,response:{result:{result:{type:(typeof v),value:(v===undefined?null:v)}}}});
        },function(e){post({cmd:tag,error:'pageeval reject: '+String(e)});});
      }catch(e){post({cmd:tag,error:'pageeval throw: '+String(e)});}
    }
          cmds.forEach(function(c){
            if(c.js){
              if(ws2&&ws2.readyState===1){send('Runtime.evaluate',{expression:c.js,returnByValue:true,awaitPromise:true},c.tag||c.js.slice(0,80),ws2);}
              else{pageeval(c.js,c.tag||c.js.slice(0,80));}
            }
            else{send(c.method,c.params,c.tag||c.method,/^(Page|Runtime|DOM|Emulation|Network)\./.test(c.method)?ws2:undefined);}
          });
        };
        x.open('GET','/queue?n='+ (poll.n=poll.n||0),true);
        x.send();
      }catch(e){}
    }
  },500);
})();
</script>'''.encode()

class H(BaseHTTPRequestHandler):
    cursor = 0
    def _reply(self, code, body, ctype='application/json'):
        b = body if isinstance(body, bytes) else body.encode()
        self.send_response(code); self.send_header('Content-Type', ctype)
        self.send_header('Content-Length', str(len(b))); self.send_header('Cache-Control', 'no-store')
        self.end_headers(); self.wfile.write(b)
    def do_GET(self):
        if self.path == '/':
            self._reply(200, PAGE, 'text/html; charset=utf-8')
        elif self.path.startswith('/queue'):
            with lock:
                served = [c for c in queue if c['id'] > H.cursor]
                if served: H.cursor = served[-1]['id']
                self._reply(200, json.dumps(served))
        elif self.path == '/results':
            with lock: self._reply(200, json.dumps(results, indent=1))
        else:
            self._reply(404, '{}')
    def do_POST(self):
        size = int(self.headers.get('Content-Length', 0))
        raw = self.rfile.read(size)
        try: body = json.loads(raw or b'{}')
        except ValueError:
            if self.path == '/result':
                body = {'cmd': 'raw-post', 'line': raw.decode('utf-8', 'replace')}
            else:
                return self._reply(400, '{}')
        if self.path == '/cmd':
            with lock:
                body['id'] = (queue[-1]['id'] + 1) if queue else 1
                queue.append(body)
                print(f"queued #{body['id']}: {body.get('method') or body.get('js','')[:60]}", flush=True)
            self._reply(200, '{"ok":true}')
        elif self.path == '/result':
            body['t'] = time.time()
            with lock:
                results.append(body)
                print('RESULT ' + json.dumps(body)[:400], flush=True)
                (OUT / f"result-{len(results):04d}.json").write_text(json.dumps(body, indent=1))
            self._reply(200, '{"ok":true}')
        else:
            self._reply(404, '{}')
    def log_message(self, *a): pass

print(f'x9 cdp shell host on http://{HOST}:{PORT}/', flush=True)
ThreadingHTTPServer((HOST, PORT), H).serve_forever()
