NGINX ?= /opt/resty_ffi/nginx/sbin/nginx

.PHONY: build
build:
	@bash build.sh

.PHONY: run
run:
	@bash -c '[[ -d logs ]] || mkdir logs'
	LD_LIBRARY_PATH=$(PWD) PYTHONPATH=$(PWD) $(NGINX) -p $(PWD) -c nginx.conf

.PHONY: test
test:
	curl localhost:20000/echo
	curl localhost:20000/kafka
