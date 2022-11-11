package resty;

public class NgxHttpLuaNonblockingFFI {
    static {
        System.loadLibrary("ffi_java");
    }
    public static native long task_poll(long p);
    public static native byte[] get_req(long p);
    public static native void respond(long p, int rc, byte[] barr);
}
