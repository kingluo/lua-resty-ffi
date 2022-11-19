NGINX ?= /opt/resty_ffi/nginx/sbin/nginx

.PHONY: build
build:
	@bash build.sh

.PHONY: run
run:
	@bash -c '[[ -d logs ]] || mkdir logs'
	CLASSPATH=$(PWD)/http2-demo/target/http2-demo-1.0-SNAPSHOT-jar-with-dependencies.jar:$(PWD)/echo-demo/target/echo-demo-1.0-SNAPSHOT-jar-with-dependencies.jar LD_LIBRARY_PATH=$(PWD) $(NGINX) -p $(PWD) -c nginx.conf

.PHONY: test
test:
	curl localhost:20000/echo
	curl localhost:20000/http2
