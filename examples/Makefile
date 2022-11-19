.PHONY: build
build:
	$(MAKE) -C go
	$(MAKE) -C rust
	$(MAKE) -C python
	$(MAKE) -C java

.PHONY: run
run:
	@bash -c '[[ -d logs ]] || mkdir logs'
	CLASSPATH=$(PWD)/java/http2-demo/target/http2-demo-1.0-SNAPSHOT-jar-with-dependencies.jar:$(PWD)/java/echo-demo/target/echo-demo-1.0-SNAPSHOT-jar-with-dependencies.jar LD_LIBRARY_PATH=/usr/lib/jvm/java-17-openjdk-amd64/lib/:/usr/lib/jvm/java-11-openjdk-amd64/lib/server:$(PWD)/go:$(PWD)/java:$(PWD)/python:$(PWD)/rust/grpc_client/target/release:$(PWD)/rust/echo/target/release PYTHONPATH=$(PWD)/python /opt/resty_ffi/nginx/sbin/nginx -p $(PWD) -c nginx.conf

.PHONY: test
test:
	curl localhost:20000/echo
