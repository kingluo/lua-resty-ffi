.PHONY: build
build:
	apt -y install python3-dev libpython3-dev
	gcc $$(python3-config --cflags) $$(python3-config --ldflags) -o libffi_python3.so main.c -lpython3.8 -fPIC -shared

.PHONY: run_demo_server
run_demo_server:
	pip3 install kafka-python cffi
	LD_LIBRARY_PATH=$(PWD) PYTHONPATH=$(PWD) /opt/nffi/nginx/sbin/nginx -p $(PWD) -c nginx.conf
