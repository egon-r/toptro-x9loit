/* Local simulation only. No real WebSocket or native Sraf API is invoked. */
(function () {
  'use strict';
  var audit = window.__x9Preview = {requests: [], queries: [], unsupported: [], launches: [], renderErrors: [], ready: false};
  // Stock logs can include parameters. Forward only error types, retaining safe
  // diagnostics for verification rather than leaking raw stock console output.
  var reportError = console.error.bind(console);
  ['log', 'info', 'debug', 'warn', 'error'].forEach(function (name) {
    console[name] = function () {
      Array.prototype.forEach.call(arguments, function (arg) {
        if (arg instanceof Error) {
          audit.renderErrors.push({name: arg.name,
            message: arg.message.replace(/(?:https?|wss?):\/\/\S+/g, '[URL]'),
            locations: (arg.stack || '').match(/(?:index|chunk-[a-z0-9]+)\.js:\d+:\d+/g) || []});
          reportError('[stock-preview] ' + arg.name);
        }
      });
    };
  });
  var overrides = {
    pid: localStorage.getItem('x9-preview-pid'),
    machine: localStorage.getItem('x9-preview-machine')
  };
  localStorage.clear();
  if (overrides.pid) { localStorage.setItem('x9-preview-pid', overrides.pid); }
  if (overrides.machine) { localStorage.setItem('x9-preview-machine', overrides.machine); }
  // The bundle derives curTheme from the simulated project ID and reloads
  // when it disagrees — never seed curTheme here, only the pid override.
  localStorage.setItem('inOobe', '0');
  // Device text is SourceHanSansCN; desktop hosts lack it. Map the stock
  // local()-only faces onto the shipped OTFs before any text renders.
  var fontFace = document.createElement('style');
  fontFace.textContent =
    '@font-face{font-family:SourceHanSansCN-Regular;src:url(/stock/static/fonts/SourceHanSansCN-Regular.11c302dd.otf) format("opentype")}' +
    '@font-face{font-family:SourceHanSansCN-Bold;src:url(/stock/static/fonts/SourceHanSansCN-Bold.8e4c4a88.otf) format("opentype")}';
  document.head.appendChild(fontFace);
  // The stock analytics initializer requires this getter even with ads absent.
  // Empty synthetic metadata deliberately supplies no device identity or tokens.
  window.sraf = {
    storage: {getOBHeaderParam: function () { return ''; }},
    tabs: {
      getTabId: function () { return 'local-navigator'; },
      hideTab: function () { document.body.style.visibility = 'hidden'; },
      showTab: function () { document.body.style.visibility = ''; }
    }
  };
  function unsupported() {
    parent.postMessage({type: 'x9-preview-unsupported'}, location.origin);
  }
  window.open = unsupported;
  var config = {
    g_hotel__hotel_mode: 0, g_hotel__hotel_swo_src: 0, g_hotel__hotel_is_swo_src: 0,
    g_gui_lang__gui_language: 'eng', g_country__country: 'USA'
  };
  var subscriptions = audit.mediaSubscriptions = [];

  function answer(request) {
    var result = {};
    var error = {code: 0, message: 'OK'};
    var method = request.method;
    audit.requests.push(method);
    if (request.params) {
      audit.queries.push({method: method, key: request.params.key, configId: request.params.configId});
    }
    if (method === 'mtk.webui.system.queryWebUIData') {
      // sw_517 selects the blue 517bg02.jpg home (user-confirmed unit skin);
      // override with localStorage x9-preview-machine before load.
      var values = {'machine:swkeystone': 0,
        'machine:type': localStorage.getItem('x9-preview-machine') || 'sw_517',
        'Common:keystone': 0, 'machine:powerPopWindow': 0};
      var keys = request.params && request.params.key || [];
      result = {};
      keys.forEach(function (key) {
        if (Object.prototype.hasOwnProperty.call(values, key)) { result[key] = values[key]; }
        else { error = {code: -32601, message: 'Unsupported preview fixture'}; }
      });
    } else if (method === 'mtk.webui.factory.sys.queryProjectId') {
      // pid∉[1,5,7,9,10,11,12,13,14,15] selects theme2 — the blue X9 skin.
      // Override by setting localStorage x9-preview-pid before load.
      result = {projectID: parseInt(localStorage.getItem('x9-preview-pid') || '2', 10)};
    } else if (method === 'mtk.webui.input.querySourceInfo') {
      result = {curSourceId: 0, list: [{index: 0, name: 'HOME', defaultSourceName: 'HOME'}]};
    } else if (method === 'mtk.webui.system.queryVersionInfo') {
      result = {serialNumber: 'X9-LOCAL-PREVIEW', version: 'LOCAL-PREVIEW'};
    } else if (method === 'mtk.webui.network.queryInternetConnection') {
      result = {enable: 'false'};
    } else if (method === 'mtk.webui.bluetooth.queryState') {
      result = {state: 'off'};
    } else if (method === 'mtk.webui.mmp.regDeviceDetect') {
      subscriptions.push(request.params.sourceType);
      result = {};
    } else if (method === 'mtk.webui.mmp.queryDeviceList') {
      result = {list: []};
    } else if (method === 'mtk.webui.time.querySyncSource') {
      result = {syncSource: 0};
    } else if (method === 'mtk.webui.time.queryUTCTime') {
      result = {seconds: Date.UTC(2026, 10, 3, 12, 0, 0) / 1000};
    } else if (method === 'mtk.webui.time.convertUTCToLocalTime') {
      result = {localSeconds: request.params.UTCSeconds};
    } else if (method === 'mtk.webui.projector.queryData' &&
        ['Common:projection_mode', 'Common:bluetoothbeepsound', 'Common:hideWowSwitch',
          'Common:supportRCtype', 'Common:isBTRCMachine'].indexOf(request.params.key) !== -1) {
      result = {val: 0};
    } else if (method === 'mtk.webui.config.setValue' &&
        Object.prototype.hasOwnProperty.call(config, request.params.configId)) {
      config[request.params.configId] = request.params.value;
      result = {};
    } else if (method === 'mtk.webui.config.queryValue' &&
        request.params.configId.every(function (key) { return Object.prototype.hasOwnProperty.call(config, key); })) {
      result = request.params.configId.map(function (key) { return {configId: key, current: config[key]}; });
    } else if (method === 'mtk.webui.launcher.GAMSendCmd' && request.params &&
        request.params.dest === 'am' && request.params.cmd === 'start' &&
        request.params.value === '[x9-sdk-hello]') {
      audit.launches.push('x9-sdk-hello');
      parent.postMessage({type: 'x9-preview-launch'}, location.origin);
      result = {};
    } else {
      error = {code: -32601, message: 'Not implemented by the local preview'};
    }
    if (error.code !== 0) {
      audit.unsupported.push(method);
      if (method === 'mtk.webui.launcher.GAMSendCmd') { unsupported(); }
    }
    return {id: request.id, error: error, result: result};
  }

  class PreviewWebSocket extends EventTarget {
    constructor(url) {
      super();
      this.url = String(url);
      this.readyState = 0;
      this.bufferedAmount = 0;
      this.protocol = '';
      this.extensions = '';
      setTimeout(() => {
        this.readyState = 1;
        this.deliver('open', new Event('open'));
      }, 0);
    }
    deliver(name, event) {
      this.dispatchEvent(event);
      if (typeof this['on' + name] === 'function') { this['on' + name](event); }
    }
    send(data) {
      if (this.readyState !== 1) { throw new Error('Preview socket is not open'); }
      var request = JSON.parse(data);
      var response = answer(request);
      setTimeout(() => this.deliver('message', new MessageEvent('message', {data: JSON.stringify(response)})), 0);
    }
    close() {
      this.readyState = 3;
      this.deliver('close', new CloseEvent('close'));
    }
  }
  ['CONNECTING', 'OPEN', 'CLOSING', 'CLOSED'].forEach(function (name, index) {
    PreviewWebSocket[name] = index;
    PreviewWebSocket.prototype[name] = index;
  });
  window.WebSocket = PreviewWebSocket;
}());
