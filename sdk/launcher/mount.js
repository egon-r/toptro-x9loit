/* Inject into the live original Vue HOME data; do not recreate its renderer. */
(function () {
  'use strict';
  var deadline = Date.now() + 15000;
  var presented = false;
  var icon = 'data:image/svg+xml,' + encodeURIComponent('<svg xmlns="http://www.w3.org/2000/svg" width="360" height="220" viewBox="0 0 360 220"><rect width="360" height="220" rx="12" fill="#11243a"/><text x="180" y="92" text-anchor="middle" font-family="sans-serif" font-size="40" fill="#6ee7d8">Hello World</text><text x="180" y="145" text-anchor="middle" font-family="sans-serif" font-size="24" fill="white">SDK · local preview</text></svg>');
  var tile = {
    key: '9001', title: 'Hello World SDK', desc: 'SDK Hello World', icon: icon, select: false,
    wbsocketParam: {method: 'mtk.webui.launcher.GAMSendCmd', params: {
      dest: 'am', cmd: 'start', ack: 'x9-sdk-hello', value: '[x9-sdk-hello]'
    }}
  };
  function findHome(vm) {
    if (!vm) { return null; }
    if (Array.isArray(vm.list)) {
      var apps = vm.list.filter(function (section) {
        return section && section.title === 'Apps' && Array.isArray(section.items) && Array.isArray(section.items[0]);
      })[0];
      if (apps) { return {vm: vm, section: apps}; }
    }
    var children = vm.$children || [];
    for (var i = 0; i < children.length; i += 1) {
      var match = findHome(children[i]);
      if (match) { return match; }
    }
    return null;
  }
  function mount() {
    var app = document.getElementById('app');
    if (app && app.__vue__ && !presented) {
      // Simulate the firmware presenting Navigator; no HDMI/boot sequence runs.
      presented = true;
      app.__vue__.$store.dispatch('app/changeSetting', {key: 'navChildrenShow', value: true});
      // Hardware source detection keeps hiding Navigator without a real TV
      // service. This UI-only harness pins the hosted Navigator view visible.
      var vm = app.__vue__;
      vm.$watch(function () { return vm.$store.getters.navChildrenShow; }, function (shown) {
        if (!shown) { vm.$store.dispatch('app/changeSetting', {key: 'navChildrenShow', value: true}); }
      });
      app.__vue__.$router.replace('/home');
    }
    var home = findHome(app && app.__vue__);
    if (home) {
      // Substitute the last app slot locally, preserving the stock column layout.
      var row = home.section.items[home.section.items.length - 1];
      row.splice(row.length - 1, 1, tile);
      home.vm.$nextTick(function () {
        window.__x9Preview.ready = true;
        parent.postMessage({type: 'x9-preview-ready'}, location.origin);
      });
    } else if (Date.now() < deadline) {
      setTimeout(mount, 50);
    } else {
      parent.postMessage({type: 'x9-preview-unsupported'}, location.origin);
    }
  }
  mount();
}());
