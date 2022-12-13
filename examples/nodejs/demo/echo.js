const { Worker, isMainThread, parentPort } = require('node:worker_threads');
const ffi = require("resty_ffi")

if (isMainThread) {
  exports.init = (cfg, tq) => {
    var worker = new Worker(__filename);
    worker.on('message', (tsk) => {
      const req = ffi.get_req(tsk)
      ffi.respond(tsk, 0, req)
    });
    worker.postMessage(tq);
    return 0;
  };
} else {
  parentPort.once('message', (tq) => {
    while (true) {
      const tsk = ffi.poll_task(tq)
      if (tsk == 0) {
        console.log('exit ffi nodejs echo runtime');
        break;
      }
      parentPort.postMessage(tsk);
    }
  });
}
