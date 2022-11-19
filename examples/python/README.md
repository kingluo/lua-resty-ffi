# Python3 ffi lib

## Build

```bash
apt install python3-dev libpython3-dev
cd examples/lib_ffi_python3
gcc $(python3-config --cflags) $(python3-config --ldflags) -o lib_ffi_python3.so main.c -lpython3.8 -fPIC -shared
```

## Test

```bash
pip3 install kafka-python cffi
cd examples/lib_ffi_python3
LD_LIBRARY_PATH=$PWD PYTHONPATH=$PWD /opt/nffi/nginx/sbin/nginx -p $PWD -c nginx.conf -g "daemon off;"

curl localhost:10000
```
