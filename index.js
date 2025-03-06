
/* eslint-disable @typescript-eslint/no-require-imports */

try {
  module.exports = require('node-gyp-build')(__dirname)
  console.log('autolib loaded native module', Object.keys(module.exports))
} catch (err) {
  console.error('autolib failed to load native module:', err)
  module.exports = {
    sendCtrlKey: function() {
      console.warn('autolib: SendCtrlKey mock')
      return 0
    }
  }
}
