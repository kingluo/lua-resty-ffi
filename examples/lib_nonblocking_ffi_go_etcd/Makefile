.PHONY: build
build:
	go mod download
	go build -buildmode=c-shared -o libffi_go_etcd.so etcd.go
	go build -buildmode=c-shared -o libffi_go_echo.so echo.go
