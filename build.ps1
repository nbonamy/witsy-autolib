
node-gyp clean
node-gyp configure
node-gyp build
npx prebuildify --napi --arch x64
npx prebuildify --napi --arch arm64