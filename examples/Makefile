.PHONY: run
run:
	@bash -c '[[ -d logs ]] || mkdir logs'
	CLASSPATH=$(PWD)/lib_nonblocking_ffi_java/http2-demo/target/http2-demo-1.0-SNAPSHOT-jar-with-dependencies.jar:$(PWD)/lib_nonblocking_ffi_java/echo-demo/target/echo-demo-1.0-SNAPSHOT-jar-with-dependencies.jar LD_LIBRARY_PATH=/usr/lib/jvm/java-17-openjdk-amd64/lib/:/usr/lib/jvm/java-11-openjdk-amd64/lib/server:$(PWD)/lib_nonblocking_ffi_go_etcd:$(PWD)/lib_nonblocking_ffi_java:$(PWD)/lib_nonblocking_ffi_python3:$(PWD)/rust/grpc_client/target/release:$(PWD)/rust/echo/target/release PYTHONPATH=$(PWD)/lib_nonblocking_ffi_python3 /opt/nffi/nginx/sbin/nginx -p $(PWD) -c nginx.conf
