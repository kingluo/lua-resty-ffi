NGINX_BIN ?= /opt/resty_ffi/nginx/sbin/nginx

.PHONY: build
build:
	cd echo; cargo build --release

.PHONY: run
run:
	@bash -c '[[ -d logs ]] || mkdir logs'
	LD_LIBRARY_PATH=$(PWD)/echo/target/release:/usr/local/lib/lua/5.1 $(NGINX_BIN) -p $(PWD) -c nginx.conf

.PHONY: test
test:
	curl localhost:20000/echo
