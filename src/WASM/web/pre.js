// QUASI88 WASM pre-run shim.
//
// Mounts IDBFS at /quasi88 with autoPersist: true. IDBFS internally
// instruments MEMFS write/close at the node-ops layer, marks files
// isModified, and on close queues an async setTimeout(0) call to
// IDBFS.syncfs. The C side (fwrite/fclose) never waits for IndexedDB;
// bursts of writes within the same event-loop tick collapse into one
// IDB transaction.
//
// Env vars must be assigned inside preRun: emscripten only aliases
// Module.ENV to the runtime $ENV table at runtime-init time, so a
// top-level assignment gets overwritten before getenv() reads it.
//   HOME              → routes ~/.quasi88/{rc,state} into the IDBFS mount.
//   QUASI88_*_DIR     → file-op.c uses these instead of its defaults
//                       (CWD for SNAP, $HOME/.quasi88/state for STATE),
//                       matching the directories we mkdir below.
Module = Module || {};

Module.preRun = (Module.preRun || []).concat([function() {
  var FS = Module.FS;

  Module.ENV.HOME              = '/quasi88';
  Module.ENV.QUASI88_SNAP_DIR  = '/quasi88/SNAP';
  Module.ENV.QUASI88_STATE_DIR = '/quasi88/STATE';

  FS.mkdir('/quasi88');
  FS.mount(IDBFS, { autoPersist: true }, '/quasi88');

  Module.addRunDependency('idbfs-load');
  FS.syncfs(true, function(err) {
    if (err) console.error('IDBFS load:', err);
    else     console.log('IDBFS load OK');

    ['ROM', 'DISK', 'TAPE', 'SNAP', 'STATE'].forEach(function(d) {
      try { FS.mkdir('/quasi88/' + d); }
      catch (e) { if (e.errno !== 20) throw e; } // 20 = EEXIST
    });
    FS.chdir('/quasi88');
    Module.removeRunDependency('idbfs-load');
  });
}]);
