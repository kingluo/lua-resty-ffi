package demo.http2;

import java.io.IOException;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpHeaders;
import java.net.http.HttpRequest;
import java.net.http.HttpRequest.BodyPublishers;
import java.net.http.HttpResponse;
import java.net.http.HttpClient.Version;
import java.util.LinkedHashMap;
import com.google.gson.Gson;
import resty.NgxHttpLuaNonblockingFFI;

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
    public long tq;

    public PollThread(long tq) {
        this.tq = tq;
    }

    public void run() {
        var httpClient = HttpClient.newBuilder().version(Version.HTTP_2).build();
        var gson = new Gson();

        while(true) {
            var task = NgxHttpLuaNonblockingFFI.task_poll(tq);
            var req = NgxHttpLuaNonblockingFFI.get_req(task);

            var data = gson.fromJson(new String(req), Request.class);
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
                    NgxHttpLuaNonblockingFFI.respond(task, 0, json.getBytes());
                });
        }
    }
}

public class App
{
    public static void init(long tq)
    {
        var pollThread = new PollThread(tq);
        Thread thread = new Thread(pollThread);
        thread.setDaemon(true);
        thread.start();
    }
}
