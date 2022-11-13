.PHONY: build
build:
	go mod download
	go build -buildmode=c-shared -o libffi_go_etcd.so etcd.go
	go build -buildmode=c-shared -o libffi_go_echo.so echo.go

.PHONY: run
run:
	@bash -c '[[ -d logs ]] || mkdir logs'
	LD_LIBRARY_PATH=$(PWD) /opt/nffi/nginx/sbin/nginx -p $(PWD) -c nginx.conf
