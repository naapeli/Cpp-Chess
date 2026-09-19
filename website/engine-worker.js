/* UCI transport: plain command strings in, plain response strings out. */
importScripts('engine.js');
let engine;
let running = false;
const commands = [];

async function drain() {
  if (!engine || running) return;
  running = true;
  try {
    while (commands.length) {
      const command = commands.shift();
      await engine.ccall('uci_command', null, ['string'], [command], { async: true });
      if (command === 'quit') { close(); return; }
    }
  } catch (error) {
    postMessage({ error: String(error) });
  } finally {
    running = false;
  }
}

onmessage = ({ data }) => {
  if (typeof data !== 'string' || /[\r\n]/.test(data)) return;
  // These commands neither mutate board state nor suspend. Everything else
  // waits until the current Asyncify call has fully unwound.
  if (engine && running && (data === 'stop' || data === 'isready')) {
    engine.ccall('uci_command', null, ['string'], [data]);
  } else {
    if (engine && running && /^(quit$|ucinewgame$|position |setoption |go )/.test(data)) {
      engine.ccall('uci_command', null, ['string'], ['stop']);
    }
    commands.push(data);
    void drain();
  }
};

createChessEngine({
  print: line => postMessage(line),
  printErr: line => postMessage({ error: line }),
}).then(instance => { engine = instance; void drain(); })
  .catch(error => postMessage({ error: `Could not load engine: ${error}` }));
