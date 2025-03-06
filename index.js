
/* eslint-disable @typescript-eslint/no-require-imports */

try {
  module.exports = require('node-gyp-build')(__dirname)
  console.log('autolib loaded native module', Object.keys(module.exports))
} catch (err) {
  console.error('autolib failed to load native module:', err)
  module.exports = {
    sendCtrlKey: function() {
      throw new Error('autolib native module not loaded')
    },
    getForemostWindow: function() {
      throw new Error('autolib native module not loaded')
    },
    getProductName: function() {
      throw new Error('autolib native module not loaded')
    },
    getApplicationIcon: function() {
      throw new Error('autolib native module not loaded')
    }
  }
}
