package demo.http2;

import jnr.ffi.LibraryLoader;
import jnr.ffi.*;
import jnr.ffi.byref.*;
import jnr.ffi.annotations.In;
import jnr.ffi.annotations.Out;

import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpHeaders;
import java.net.http.HttpRequest;
import java.net.http.HttpRequest.BodyPublishers;
import java.net.http.HttpResponse;
import java.net.http.HttpClient.Version;
import java.nio.charset.StandardCharsets;
import java.util.LinkedHashMap;

import com.google.gson.Gson;

class Request {
    public String method;
    public String uri;
    public LinkedHashMap<String, String> headers;
    public String body;
}

class Response {
    public LinkedHashMap<String, String> headers = new LinkedHashMap<>();
    public String body;
}

class PollThread implements Runnable {
    public interface FFI {
        Pointer ngx_http_lua_ffi_task_poll(@In Pointer tq);
        Pointer ngx_http_lua_ffi_get_req(@In Pointer task, @Out IntByReference len);
        void ngx_http_lua_ffi_respond(@In Pointer tsk, @In int rc, @In Pointer rsp, @In int rsp_len);
    }

    public interface Libc {
        Pointer malloc(int size);
    }

    public long tq;

    public PollThread(long tq) {
        this.tq = tq;
    }

    public void run() {
        Libc libc = LibraryLoader.create(Libc.class).load("c");
        FFI ffi = LibraryLoader.create(FFI.class).load(LibraryLoader.DEFAULT_LIBRARY);
        var tq = Pointer.wrap(jnr.ffi.Runtime.getSystemRuntime(), this.tq);
        IntByReference ap = new IntByReference();

        var httpClient = HttpClient.newBuilder().version(Version.HTTP_2).build();
        var gson = new Gson();

        while(true) {
            var task = ffi.ngx_http_lua_ffi_task_poll(tq);
            if (task == null) {
                break;
            }
            var req = ffi.ngx_http_lua_ffi_get_req(task, ap);
            var len = ap.intValue();

            final byte[] bytes = new byte[len];
            req.get(0, bytes, 0, len);
            String str = new String(bytes, StandardCharsets.UTF_8);

            var data = gson.fromJson(str, Request.class);
            var builder = HttpRequest.newBuilder().uri(URI.create(data.uri));
            builder.method(data.method, BodyPublishers.ofString(data.body));
            if (data.headers != null) {
                for (var entry : data.headers.entrySet()) {
                    builder.header(entry.getKey(), entry.getValue());
                }
            }

            httpClient.sendAsync(builder.build(), HttpResponse.BodyHandlers.ofString())
                .thenAccept(res -> {
                    Response rsp = new Response();
                    HttpHeaders headers = res.headers();
                    headers.map().forEach((key, values) -> {
                        rsp.headers.put(key, String.join(",", values));
                    });
                    rsp.body = res.body();
                    String json = gson.toJson(rsp);

                    byte[] arr = json.getBytes();
                    int rspLen = arr.length;
                    var rspStr = libc.malloc(rspLen);
                    rspStr.put(0, arr, 0, rspLen);
                    ffi.ngx_http_lua_ffi_respond(task, 0, rspStr, rspLen);
                });
        }
    }
}

public class App
{
    public static int init(String cfg, long tq)
    {
        var pollThread = new PollThread(tq);
        Thread thread = new Thread(pollThread);
        thread.setDaemon(true);
        thread.start();
        return 0;
    }
}
