(function () {
  'use strict';
  var root = document.getElementById('app');
  var refresh = document.getElementById('refresh');
  var counter = document.getElementById('counter');
  var network = document.getElementById('network');
  var count = 0;
  var loading = false;

  function loadGreeting() {
    if (loading) { return; }
    loading = true;
    network.textContent = 'Loading…';
    X9.net.getJSON('/api/hello', function (error, data) {
      loading = false;
      if (error) {
        network.textContent = 'Request failed: ' + error.message;
      } else if (!data || typeof data.message !== 'string') {
        network.textContent = 'Request failed: expected a greeting message';
      } else {
        network.textContent = data.message;
      }
    });
  }
  refresh.addEventListener('click', loadGreeting);
  counter.addEventListener('click', function () {
    count += 1;
    counter.textContent = 'Count: ' + count;
  });
  X9.input.attach(root);
  loadGreeting();
}());
