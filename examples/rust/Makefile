.PHONY: build
build:
	cd echo; cargo build --release
	cd grpc_client; cargo build --release

.PHONY: run_demo_server
run_demo_server:
	LD_LIBRARY_PATH=$(PWD)/grpc_client/target/release:$(PWD)/echo/target/release /opt/nffi/nginx/sbin/nginx -p $(PWD) -c nginx.conf
