NGINX_BIN ?= /opt/resty_ffi/nginx/sbin/nginx

.PHONY: build
build:
	go mod download
	go build -buildmode=c-shared -o libffi_go_echo.so echo.go

.PHONY: run
run:
	@bash -c '[[ -d logs ]] || mkdir logs'
	LD_LIBRARY_PATH=$(PWD) $(NGINX_BIN) -p $(PWD) -c nginx.conf

.PHONY: test
test:
	curl localhost:20000/echo
