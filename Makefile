.DEFAULT_GOAL := libresty_ffi.so

include $(NGX_OBJS_MAKEFILE)

libresty_ffi.so:
	$(CC) $(CFLAGS) $(ALL_INCS) -I$(NGX_LUA_SRC) -ggdb3 -shared -fPIC -DSHARED_OBJECT -ldl \
		$(SRC)/build-id.c $(SRC)/ngx_http_lua_ffi.c -o $(SRC)/$@
