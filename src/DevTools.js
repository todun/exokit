const http = require('http');
const path = require('path');
const fs = require('fs');
const os = require('os');
const htermRepl = require('hterm-repl');

const DOM = require('./DOM');
const {HTMLIframeElement} = DOM;

const DEVTOOLS_PORT = 9223;

const dataPath = (() => {
  const candidatePathPrefixes = [
    os.homedir(),
    __dirname,
    os.tmpdir(),
  ];
  for (let i = 0; i < candidatePathPrefixes.length; i++) {
    const candidatePathPrefix = candidatePathPrefixes[i];
    if (candidatePathPrefix) {
      const ok = (() => {
        try {
          fs.accessSync(candidatePathPrefix, fs.constants.W_OK);
          return true;
        } catch(err) {
          return false;
        }
      })();
      if (ok) {
        return path.join(candidatePathPrefix, '.exokit');
      }
    }
  }
  return null;
})();
function cleanPipeName(str) {
    if (process.platform === 'win32') {
        str = str.replace(/^\//, '');
        str = str.replace(/\//g, '-');
        return '\\\\.\\pipe\\'+str;
    } else {
        return str;
    }
}
const socketPath = cleanPipeName(path.join(dataPath, 'unix.socket'));

console.log('got path', socketPath);

const _getReplServer = (() => {
  let replServer = null;
  return () => new Promise((accept, reject) => {
    if (!replServer) {
      const newReplServer = http.createServer((req, res) => {
        console.log('got req', req.url);
        req.pipe(process.stdout);
        req.on('end', () => {
          console.log('req end');
          res.end('zol');
        });
      });
      newReplServer.listen(socketPath, err => {
        if (!err) {
          replServer = newReplServer;
          // XXX compute the url to use
          accept(replServer);
        } else  {
          reject(err);
        }
      });
      /* htermRepl({
        port: DEVTOOLS_PORT,
      }, (err, newReplServer) => {
        if (!err) {
          replServer = newReplServer;
          // XXX compute the url to use
          accept(replServer);
        } else  {
          reject(err);
        }
      }); */
    } else {
      accept(replServer);
    }
  });
})();

let id = 0;
class DevTools {
  constructor(context, replServer) {
    this.context = context;
    this.replServer = replServer;
    this.id = (++id) + '';
    this.repls = [];

    this.onRepl = this.onRepl.bind(this);
    this.replServer.on('repl', this.onRepl);
  }

  getPath() {
    return `/?id=${this.id}`;
  }
  getHost() {
    /* // localhost is not accessible on all platforms
    const interfaceClasses = os.networkInterfaces();
    for (const k in interfaceClasses) {
      const interfaces = interfaceClasses[k];
      for (let i = 0; i < interfaces.length; i++) {
        const iface = interfaces[i];
        if (!iface.internal) {
          return iface.address;
        }
      }
    }
    return '127.0.0.1'; */
    return '192.168.0.14';
  }
  getUrl() {
    console.log('try');
    /* http.request({
      socketPath,
    }, (res) => {
      res.pipe(process.stdout);
    }).end(); */
    return 'http://127.0.0.1:8000/';
    // return socketPath;
  }

  onRepl(r) {
    if (r.url === this.getPath()) {
      r.setEval((s, context, filename, cb) => {
        console.log('eval 1', JSON.stringify(s));
        let err = null, result;
        try {
          result = this.context.vm.run(s);
        } catch (e) {
          err = e;
        }
        console.log('eval 2', err, result);
        if (!err) {
          cb(null, result);
        } else {
          cb(err);
        }
      });
    }
  }

  destroy() {
    this.replServer.removeListener('repl', this.onRepl);

    for (let i = 0; i < this.repls.length; i++) {
      this.repls[i].close();
    }
    this.repls.length = 0;
  }
}

module.exports = {
  async requestDevTools(iframe) {
    // console.log('network interfaces', os.platform(), JSON.stringify(os.networkInterfaces(), null, 2));
    
    const replServer = await _getReplServer();
    return new DevTools(iframe, replServer);
  },
};
