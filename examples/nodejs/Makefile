NGINX_BIN ?= /opt/resty_ffi/nginx/sbin/nginx

.PHONY: build
build:
	@bash build.sh

.PHONY: run
run:
	@bash -c '[[ -d logs ]] || mkdir logs'
	LD_LIBRARY_PATH=$(PWD):/usr/local/lib/:/usr/local/lib/lua/5.1 NODE_PATH=$(PWD):$(PWD)/build/Release $(NGINX_BIN) -p $(PWD) -c nginx.conf

.PHONY: test
test:
	curl localhost:20000/echo
	curl localhost:20000/http2
