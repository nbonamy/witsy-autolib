
/* eslint-disable @typescript-eslint/no-require-imports */

try {
  module.exports = require('node-gyp-build')(__dirname)
} catch (err) {
  console.error('autolib failed to load native module:', err)
  module.exports = {
    sendKey: function() {
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
