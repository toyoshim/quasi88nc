var nacl = document.querySelector('#nacl_module');
var pbar = document.querySelector('#progress-bar');
var progress = document.querySelector('#progress');
var ready = document.querySelector('#ready');
var menu = document.querySelector('#menu');
var file = document.querySelector('#file');
var settings = document.querySelector('#settings');

var roms = [
  "N88.ROM",
  "N88EXT0.ROM",
  "N88EXT1.ROM",
  "N88EXT2.ROM",
  "N88EXT3.ROM",
  "N88N.ROM",
  "N88SUB.ROM",
  "N88KNJ1.ROM",
  "N88KNJ2.ROM",
  "N88JISHO.ROM",
  "FONT.ROM"
];

var romReady = false;
var pfsReady = false;
var naclReady = false;

var setRomReady = function() {
  settings.style.setProperty('display', 'none');
  if (romReady)
    return;
  romReady = true;
  if (naclReady)
  	nacl.postMessage('_start_');
}

var romCheck = function() {
  for (var i = 1; i <= 11; ++i) {
    var name = "#rom" + i + "s";
    var status = document.querySelector(name);
    if (status.innerText != 'found' && status.innerText != 'optional')
      return false;
  }
  ready.disabled = false;
  return true;
}

var initRomChooser = function() {
  for (var i = 1; i <= 11; ++i) {
  	var name = "#rom" + i;
  	var button = document.querySelector(name);
  	button._index = i - 1;
  	button._name = roms[button._index];
    romRoot.getFile(button._name, {create: false}, function() {
      var status = document.querySelector('#' + this.id + 's');
      status.innerText = 'found';
      if (romCheck())
        setRomReady();
    }.bind(button));
  	button.addEventListener('click', function(e) {
  	  if (!pfsReady)
  	    return;
  	  var options = {
  	    type: 'openFile',
  	    suggestedName: button._name,
  	    accepts: [{extensions: ['rom']}],
  	    acceptsAllTypes: false
  	  };
  	  chrome.fileSystem.chooseEntry(options, function(rentry) {
        if (!rentry)
          return;
        romRoot.getFile(
            this._name, {create: true, exclusive: false}, function(wentry) {
          wentry.createWriter(function(writer) {
            rentry.file(function(file) {
              writer.onwriteend = function(e) {
                var status = document.querySelector('#' + this.id + 's');
                status.innerText = 'found';
                romCheck();
              }.bind(this);
              writer.write(file);
            }.bind(this), perror.bind("file"));
          }.bind(this), perror.bind("createWriter"));
        }.bind(this), perror.bind("getFile"));
  	  }.bind(this));
  	}, false);
  }
}

var pfs = null;
var romRoot = null;
var diskRoot = null;
var requestFileSystem =
    window.requestFileSystem || window.webkitRequestFileSystem;
var perror = function(e) { e.where = this.toString(); console.error(e); }
requestFileSystem(PERSISTENT, 1024 * 1024 * 1024, function(fs) {
  pfs = fs;
  pfs.root.getDirectory('QUASI88', {create: true}, function(entry) {
    entry.getDirectory('ROM', {create: true}, function(entry) {
      romRoot = entry;
      pfsReady = diskRoot != null;
      initRomChooser();
    });
    entry.getDirectory('DISK', {create: true}, function(entry) {
      diskRoot = entry;
      pfsReady = romRoot != null;
    });
  }, perror.bind("getDirectory"));
}, perror.bind("requestFileSystem"));

var updateProgress = function(rate) {
  var n = "" + (rate * 100);
  pbar.style.width = n + "%";
  pbar.setAttribute('aria-valuenow', n);
  if (rate != 1)
    return;
  progress.className = "progress";
}

nacl.addEventListener('load', function(e) {
  updateProgress(1);
  if (naclReady)
    return;
  naclReady = true;
  if (romReady)
  	  nacl.postMessage('_start_');
}, false);

nacl.addEventListener('progress', function(e) {
  if (e.total == 0)
    updateProgress(1);
  else
    updateProgress(e.loaded / e.total);
}, false);

ready.addEventListener('click', function(e) {
  setRomReady();
}, false);

rom.addEventListener('click', function(e) {
  settings.style.setProperty('display', 'block');
}, false);

menu.addEventListener('click', function(e) {
  if (!romReady)
    return;
  nacl.postMessage('_config_');
}, false);

file.addEventListener('click', function(e) {
  if (!pfsReady)
    return;
  var options = {
    type: 'openFile',
  	accepts: [{extensions: ['d88']}],
  	acceptsAllTypes: false
  };
  chrome.fileSystem.chooseEntry(options, function(rentry) {
    if (!rentry)
      return;
    console.log(rentry);
    diskRoot.getFile(
        rentry.name, {create: true, exclusive: false}, function(wentry) {
      wentry.createWriter(function(writer) {
        rentry.file(function(file) {
          writer.onwriteend = function(e) {};
          writer.write(file);
        }.bind(this), perror.bind("file"));
      }.bind(this), perror.bind("createWriter"));
    }.bind(this), perror.bind("getFile"));
  }.bind(this));
}, false);
