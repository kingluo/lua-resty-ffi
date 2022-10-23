package demo.echo;

import resty.NgxHttpLuaNonblockingFFI;

class PollThread implements Runnable {
    public long tq;

    public PollThread(long tq) {
        this.tq = tq;
    }

    public void run() {
        while(true) {
            var task = NgxHttpLuaNonblockingFFI.task_poll(tq);
            if (task == 0) {
                break;
            }
            var req = NgxHttpLuaNonblockingFFI.get_req(task);
            String rsp = new String(req);
            NgxHttpLuaNonblockingFFI.respond(task, 0, rsp.getBytes());
        }
        System.out.println("exit java echo runtime");
    }
}

public class App
{
    public static int init(long tq, String configStr) {
        var pollThread = new PollThread(tq);
        Thread thread = new Thread(pollThread);
        thread.setDaemon(true);
        thread.start();
        return 0;
    }
}
