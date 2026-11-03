/* X9 web SDK. Standard browser APIs only; no native Sraf privileges. */
(function (global) {
  'use strict';

  function getJSON(url, callback) {
    var xhr = new XMLHttpRequest();
    var finished = false;
    function finish(error, value) {
      if (finished) { return; }
      finished = true;
      callback(error, value);
    }
    xhr.open('GET', url, true);
    xhr.timeout = 5000;
    xhr.onload = function () {
      var value;
      if (xhr.status < 200 || xhr.status >= 300) {
        finish(new Error('HTTP ' + xhr.status));
        return;
      }
      try { value = JSON.parse(xhr.responseText); }
      catch (error) { finish(new Error('Invalid JSON response')); return; }
      finish(null, value);
    };
    xhr.onerror = function () { finish(new Error('Network request failed')); };
    xhr.ontimeout = function () { finish(new Error('Network request timed out')); };
    xhr.onabort = function () { finish(new Error('Network request aborted')); };
    xhr.send();
    return xhr;
  }

  // Ordered button navigation: left/up previous, right/down next, Enter activates.
  // Pointer clicks remain native; do not turn every page click into activation.
  function attachInput(root) {
    function buttons() {
      return Array.prototype.filter.call(root.querySelectorAll('button'), function (button) {
        return !button.disabled && button.getClientRects().length > 0;
      });
    }
    function onKey(event) {
      // A background click can leave BODY focused; allow arrow-key recovery.
      if (event.target !== document.body && !root.contains(event.target)) { return; }
      var code = event.keyCode;
      var list = buttons();
      var index = list.indexOf(document.activeElement);
      if (event.altKey || event.ctrlKey || event.metaKey || !list.length) { return; }
      if (code === 13 && index !== -1) {
        event.preventDefault();
        if (!event.repeat) { list[index].click(); }
      } else if (code >= 37 && code <= 40) {
        event.preventDefault();
        index = index === -1 ? 0 : Math.max(0, Math.min(list.length - 1,
          index + (code === 37 || code === 38 ? -1 : 1)));
        list[index].focus();
      }
    }
    document.addEventListener('keydown', onKey);
    var initial = buttons();
    if (initial.length) { initial[0].focus(); }
    return function () { document.removeEventListener('keydown', onKey); };
  }

  global.X9 = {net: {getJSON: getJSON}, input: {attach: attachInput}};
}(window));
