chrome.app.runtime.onLaunched.addListener(function(data) {
  var options = {
    resizable: false,
    bounds: {
      width: 640,
      height: 420
    }
  };
  var onOpen = function (window) {
    if (!this.items || !this.items[0])
      return;
    window.contentWindow._preload = this.items[0];
  }.bind(data);
  chrome.app.window.create('window.html', options, onOpen);
});
