NGINX ?= /opt/resty_ffi/nginx/sbin/nginx

.PHONY: build
build:
	apt -y install python3-dev
	gcc $$(python3-config --cflags) $$(python3-config --ldflags) -o libffi_python3.so loader.c \
		-lpython$$(python3 -c 'import sys; print(".".join(map(str, sys.version_info[:2])))') \
		-fPIC -shared -pthread
	pip3 install -r requirements.txt

.PHONY: run
run:
	@bash -c '[[ -d logs ]] || mkdir logs'
	LD_LIBRARY_PATH=$(PWD) PYTHONPATH=$(PWD) $(NGINX) -p $(PWD) -c nginx.conf

.PHONY: test
test:
	curl localhost:20000/echo
	curl localhost:20000/kafka
