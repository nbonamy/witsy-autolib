const autolib = require('./index.js');

console.log('Starting key monitor test...');
console.log('Press some keys. Press Ctrl+C to exit.');
console.log('');

// macOS key codes for reference:
// Right Command: 54
// Left Command: 55
// Right Shift: 60
// Left Shift: 56
// Right Option: 61
// Left Option: 58
// Right Control: 62
// Left Control: 59

// Track modifier state for "pressed alone" detection
let modifierPressedAt = {};
let otherKeyPressed = false;

const result = autolib.startKeyMonitor((event) => {
  console.log('Key event:', JSON.stringify(event, null, 2));

  // Detect right Command pressed alone
  if (event.type === 'flagsChanged') {
    const rightCommandMask = 0x10; // Right command flag in lower bits
    const commandMask = 0x100000; // Command modifier active

    // Check if right command was just pressed
    if ((event.flags & commandMask) && event.keyCode === 54) {
      modifierPressedAt[54] = Date.now();
      otherKeyPressed = false;
      console.log('>>> Right Command pressed');
    }
    // Check if right command was just released
    else if (!(event.flags & commandMask) && modifierPressedAt[54]) {
      const heldTime = Date.now() - modifierPressedAt[54];
      if (!otherKeyPressed && heldTime < 500) {
        console.log('>>> RIGHT COMMAND PRESSED ALONE! <<<');
      }
      delete modifierPressedAt[54];
    }
  }

  // Track if other keys were pressed while modifier held
  if (event.type === 'down' && Object.keys(modifierPressedAt).length > 0) {
    otherKeyPressed = true;
  }
});

if (result === 0) {
  console.log('Key monitor started successfully');
} else if (result === 3) {
  console.log('Failed to start key monitor. Please grant accessibility permissions.');
  console.log('System Preferences > Security & Privacy > Privacy > Accessibility');
  process.exit(1);
} else {
  console.log('Failed to start key monitor. Error code:', result);
  process.exit(1);
}

// Handle Ctrl+C
process.on('SIGINT', () => {
  console.log('\nStopping key monitor...');
  autolib.stopKeyMonitor();
  console.log('Done.');
  process.exit(0);
});
