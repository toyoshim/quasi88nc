chrome.app.runtime.onLaunched.addListener(function() {
  var options = {
    resizable: false,
    bounds: {
      width: 640,
      height: 420
    }
  };
  var onOpen = function () {
  };
  chrome.app.window.create('window.html', options, onOpen);
});
