
.DEFAULT_GOAL := all

all:
	npx node-gyp clean
	npx node-gyp configure
	npx node-gyp build
	npx prebuildify --napi --arch=x64
	npx prebuildify --napi --arch=arm64