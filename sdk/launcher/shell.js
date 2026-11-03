(function () {
  'use strict';
  var stock = document.getElementById('stock');
  var sample = document.getElementById('sample');
  var back = document.getElementById('back');
  var notice = document.getElementById('notice');
  function fit() {
    var stage = document.getElementById('stage');
    stock.style.transform = 'scale(' + Math.min(stage.clientWidth / 1920, stage.clientHeight / 1080) + ')';
  }
  function home() {
    sample.hidden = true;
    sample.removeAttribute('src');
    stock.hidden = false;
    back.hidden = true;
    document.getElementById('mode').textContent = 'Original UI / simulated firmware services';
    notice.textContent = 'Returned to the original launcher. No native process was started.';
    stock.contentWindow.focus();
  }
  back.addEventListener('click', home);
  window.addEventListener('message', function (event) {
    if (event.origin !== location.origin || event.source !== stock.contentWindow) { return; }
    if (event.data && event.data.type === 'x9-preview-launch') {
      stock.hidden = true;
      sample.hidden = false;
      sample.src = '/sample/';
      back.hidden = false;
      document.getElementById('mode').textContent = 'SDK sample / local HTTP backend';
      notice.textContent = 'Simulated launch from the original stock tile. Use Back to launcher to return.';
    } else if (event.data && event.data.type === 'x9-preview-unsupported') {
      notice.textContent = 'This firmware operation or stock app is unavailable in the local preview.';
    } else if (event.data && event.data.type === 'x9-preview-ready') {
      notice.textContent = 'Original launcher ready. Select the Hello World SDK tile.';
    }
  });
  window.addEventListener('resize', fit);
  fit();
}());
