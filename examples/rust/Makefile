NGINX ?= /opt/resty_ffi/nginx/sbin/nginx

.PHONY: build
build:
	cd echo; cargo build --release
	cd grpc_client; cargo build --release

.PHONY: run
run:
	luarocks install lua-protobuf
	@bash -c '[[ -d logs ]] || mkdir logs'
	LD_LIBRARY_PATH=$(PWD)/grpc_client/target/release:$(PWD)/echo/target/release $(NGINX) -p $(PWD) -c nginx.conf

.PHONY: test
test:
	curl localhost:20000/echo
	curl localhost:20000/say_hello
	curl localhost:20000/get_feature
	curl localhost:20000/list_features
	curl localhost:20000/record_route
	curl localhost:20000/route_chat
