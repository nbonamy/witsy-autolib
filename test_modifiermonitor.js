// Run inside Electron (needs the AppKit run loop):
//   open -n -a node_modules/electron/dist/Electron.app --args $PWD/test_modifiermonitor.js
// Press modifier keys; events are appended to /tmp/modifiermonitor.log.
// No Input Monitoring or Accessibility permission is needed.
const { app } = require('electron');
const fs = require('fs');
const autolib = require('./build/Release/autolib.node');

const log = (line) => fs.appendFileSync('/tmp/modifiermonitor.log', `${line}\n`);
fs.writeFileSync('/tmp/modifiermonitor.log', '');

app.whenReady().then(() => {
  app.dock?.hide();
  const result = autolib.startModifierMonitor((event) => log(JSON.stringify(event)));
  log(`started result=${result} running=${autolib.isModifierMonitorRunning()} again=${autolib.startModifierMonitor(() => {})}`);
  setTimeout(() => {
    log(`stop result=${autolib.stopModifierMonitor()} running=${autolib.isModifierMonitorRunning()} stopAgain=${autolib.stopModifierMonitor()}`);
    app.quit();
  }, 25000);
});
