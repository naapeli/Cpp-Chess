export class UciClient {
  constructor(onError) {
    this.worker = new Worker(new URL('./engine-worker.js', import.meta.url));
    this.waiters = new Set();
    this.pending = null;
    this.error = null;
    this.worker.onerror = event => this.fail(new Error(event.message || 'Engine worker failed'), onError);
    this.worker.onmessage = ({ data }) => {
      if (typeof data !== 'string') { this.fail(new Error(data.error), onError); return; }
      if (data.startsWith('info string error:') || data.startsWith('info string search error:')) {
        this.error = new Error(data);
      }
      for (const waiter of [...this.waiters]) {
        if (data.startsWith(waiter.prefix)) { this.waiters.delete(waiter); waiter.resolve(data); }
      }
      if (data.startsWith('info ') && this.pending) this.pending.onInfo(data);
      if (data.startsWith('bestmove ') && this.pending) {
        const pending = this.pending;
        this.pending = null;
        clearTimeout(pending.timer);
        if (this.error) pending.reject(this.error);
        else pending.resolve(data.split(/\s+/)[1]);
      }
    };
    this.ready = this.handshake();
  }
  send(command) { this.worker.postMessage(command); }
  wait(prefix, timeout = 15000) {
    return new Promise((resolve, reject) => {
      const waiter = { prefix, reject, resolve: value => { clearTimeout(timer); resolve(value); } };
      const timer = setTimeout(() => { this.waiters.delete(waiter); reject(new Error(`Engine timed out waiting for ${prefix}`)); }, timeout);
      waiter.reject = error => { clearTimeout(timer); reject(error); };
      this.waiters.add(waiter);
    });
  }
  async handshake() {
    const initialized = this.wait('uciok');
    this.send('uci');
    await initialized;
    await this.sync();
  }
  async sync() {
    const ready = this.wait('readyok');
    this.send('isready');
    await ready;
    if (this.error) throw this.error;
  }
  async position(command, newGame = false) {
    await this.ready;
    if (this.pending) throw new Error('Stop the current search before changing positions');
    this.error = null;
    if (newGame) this.send('ucinewgame');
    this.send(command);
    await this.sync();
  }
  search(milliseconds, onInfo) {
    if (this.pending) throw new Error('A search is already running');
    this.error = null;
    const promise = new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.worker.terminate();
        this.fail(new Error('Engine search timed out; reload the page to restart it'));
      }, milliseconds + 15000);
      this.pending = { resolve, reject, onInfo, timer };
      this.send(`go movetime ${milliseconds}`);
    });
    this.searchDone = promise;
    return promise;
  }
  async stop() {
    if (!this.pending) return;
    this.send('stop');
    await this.searchDone;
  }
  fail(error, onError) {
    this.error = error;
    for (const waiter of this.waiters) waiter.reject(error);
    this.waiters.clear();
    if (this.pending) { clearTimeout(this.pending.timer); this.pending.reject(error); this.pending = null; }
    onError?.(error);
  }
}
