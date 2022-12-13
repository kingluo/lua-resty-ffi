const { Worker, isMainThread, parentPort } = require('node:worker_threads');
const ffi = require("resty_ffi")
const http2 = require('node:http2');

if (isMainThread) {
  exports.init = (cfg, tq) => {
    var worker = new Worker(__filename);
    worker.on('message', (tsk) => {
      var req_data = ffi.get_req(tsk)
      req_data = JSON.parse(req_data)

      const client = http2.connect(req_data.host);
      client.on('error', (err) => {
        console.error(err)
      });

      var rsp = {'headers': {}}
      const req = client.request({ ':path': req_data.path });
      req.on('response', (headers, flags) => {
        for (const name in headers) {
          rsp['headers'][name] = headers[name]
        }
      });
      req.setEncoding('utf8');
      let data = '';
      req.on('data', (chunk) => { data += chunk; });
      req.on('end', () => {
        rsp['body'] = data
        ffi.respond(tsk, 0, JSON.stringify(rsp))
        client.close();
      });
      req.end();
    });
    worker.postMessage(tq);
    return 0;
  };
} else {
  parentPort.once('message', (tq) => {
    while (true) {
      const tsk = ffi.poll_task(tq)
      if (tsk == 0) {
        console.log('exit ffi nodejs http2 runtime');
        break;
      }
      parentPort.postMessage(tsk);
    }
  });
}
