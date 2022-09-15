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
			var req = NgxHttpLuaNonblockingFFI.get_req(task);
            String rsp = new String(req);
            NgxHttpLuaNonblockingFFI.respond(task, 0, rsp.getBytes());
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
