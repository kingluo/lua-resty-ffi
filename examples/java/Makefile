.PHONY: build
build:
	apt -y install openjdk-17-jdk
	gcc -O2 -shared -fPIC -o libffi_java.so loader.c -I/usr/lib/jvm/java-17-openjdk-amd64/include/ -I/usr/lib/jvm/java-17-openjdk-amd64/include/linux/ -L/usr/lib/jvm/java-17-openjdk-amd64/lib/ -L/usr/lib/jvm/java-17-openjdk-amd64/lib/server/ -ljli -ljvm -pthread
	cd http2-demo; mvn -q -DskipTests=true -Dmaven.test.skip=true package
	cd echo-demo; mvn -q -DskipTests=true -Dmaven.test.skip=true package

.PHONY: run
run:
	@bash -c '[[ -d logs ]] || mkdir logs'
	CLASSPATH=$(PWD)/http2-demo/target/http2-demo-1.0-SNAPSHOT-jar-with-dependencies.jar:$(PWD)/echo-demo/target/echo-demo-1.0-SNAPSHOT-jar-with-dependencies.jar LD_LIBRARY_PATH=/usr/lib/jvm/java-17-openjdk-amd64/lib/:/usr/lib/jvm/java-11-openjdk-amd64/lib/server:$(PWD) /opt/nffi/nginx/sbin/nginx -p $(PWD) -c nginx.conf

.PHONY: test
test:
	curl localhost:20000/echo
	curl localhost:20000/http2
