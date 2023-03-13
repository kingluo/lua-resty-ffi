NGINX_BIN ?= /opt/resty_ffi/nginx/sbin/nginx

.PHONY: build
build:
	@bash build.sh

.PHONY: run
run:
	pip3 install -r requirements.txt
	@bash -c '[[ -d logs ]] || mkdir logs'
	LD_LIBRARY_PATH=$(PWD):/usr/local/lib/lua/5.1 PYTHONPATH=$(PWD) $(NGINX_BIN) -p $(PWD) -c nginx.conf

.PHONY: test
test:
	curl localhost:20000/echo
	curl localhost:20000/kafka
