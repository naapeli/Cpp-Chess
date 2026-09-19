import { Chess, DEFAULT_POSITION } from './vendor/chess.js';
import { UciClient } from './uci-client.js';

const $ = id => document.getElementById(id);
const names = { p: 'pawn', n: 'knight', b: 'bishop', r: 'rook', q: 'queen', k: 'king' };
const glyphs = { p: '♟', n: '♞', b: '♝', r: '♜', q: '♛', k: '♚' };
let game = new Chess();
let rootFen = DEFAULT_POSITION;
let moves = [];
let player = 'w';
let orientation = 'w';
let selected = null;
let ready = false;
let busy = false;
let changing = false;
let generation = 0;
let promotionChoice;
let lastMove = null;
let selfPlay = false;
let autoTimer;
let fenDirty = false;
const engine = new UciClient(showError);

function showError(error) { $('error').textContent = error.message || String(error); $('error').hidden = false; }
function clearError() { $('error').hidden = true; }
function positionCommand() { return `position fen ${rootFen}${moves.length ? ` moves ${moves.join(' ')}` : ''}`; }
function colorName(color) { return color === 'w' ? 'White' : 'Black'; }
function canMove() { return ready && !busy && !changing && !selfPlay && !game.isGameOver() && game.turn() === player; }

function render() {
  if (game.isGameOver()) { selfPlay = false; clearTimeout(autoTimer); }
  const targets = selected ? game.moves({ square: selected, verbose: true }).map(move => move.to) : [];
  const files = [...'abcdefgh'];
  const ranks = [...'87654321'];
  if (orientation === 'b') { files.reverse(); ranks.reverse(); }
  const board = $('board');
  board.replaceChildren();
  for (const rank of ranks) for (const file of files) {
    const square = file + rank;
    const piece = game.get(square);
    const cell = document.createElement('button');
    cell.className = `square${(file.charCodeAt(0) - 97 + Number(rank)) % 2 ? ' dark' : ''}`;
    cell.dataset.square = square;
    cell.setAttribute('aria-label', `${square}${piece ? ` ${colorName(piece.color)} ${names[piece.type]}` : ' empty'}`);
    cell.setAttribute('aria-pressed', String(selected === square));
    cell.classList.toggle('selected', selected === square);
    cell.classList.toggle('legal', targets.includes(square));
    cell.classList.toggle('last', lastMove?.from === square || lastMove?.to === square);
    cell.classList.toggle('check', piece?.type === 'k' && piece.color === game.turn() && game.isCheck());
    if (piece) {
      const span = document.createElement('span');
      span.className = `piece ${piece.color === 'w' ? 'white' : 'black'}`;
      span.textContent = glyphs[piece.type];
      span.setAttribute('aria-hidden', 'true');
      cell.append(span);
    }
    for (const [display, text, className] of [[file === files[0], rank, 'rank'], [rank === ranks[7], file, 'file']]) {
      if (!display) continue;
      const coordinate = document.createElement('span');
      coordinate.className = `coordinate ${className}`;
      coordinate.textContent = text;
      coordinate.setAttribute('aria-hidden', 'true');
      cell.append(coordinate);
    }
    cell.draggable = canMove() && piece?.color === player;
    cell.addEventListener('click', () => void selectSquare(square));
    cell.addEventListener('dragstart', event => {
      if (!canMove() || piece?.color !== player) { event.preventDefault(); return; }
      // Do not replace the dragged DOM element before the drop finishes.
      selected = square;
      event.dataTransfer.setData('text/plain', square);
    });
    cell.addEventListener('dragover', event => { if (canMove()) event.preventDefault(); });
    cell.addEventListener('drop', event => {
      event.preventDefault();
      if (!canMove()) return;
      selected = event.dataTransfer.getData('text/plain');
      void selectSquare(square);
    });
    board.append(cell);
  }
  let status = !ready ? 'Loading engine…' : busy ? 'Engine is thinking…' : `${colorName(game.turn())} to move`;
  if (game.isCheckmate()) status = `Checkmate. ${colorName(game.turn() === 'w' ? 'b' : 'w')} wins.`;
  else if (game.isStalemate()) status = 'Draw by stalemate.';
  else if (game.isThreefoldRepetition()) status = 'Draw by threefold repetition.';
  else if (game.isInsufficientMaterial()) status = 'Draw by insufficient material.';
  else if (game.isDraw()) status = 'Draw.';
  else if (!busy && game.isCheck()) status += ' · Check';
  if (selfPlay) status += ' · Self-play';
  else if (ready && !busy && !game.isGameOver()) status += game.turn() === player ? ' · Your turn' : ' · Press Engine move to continue';
  $('status').textContent = status;
  $('thinking').textContent = !ready ? 'Loading' : busy ? 'Thinking' : 'Ready';
  $('thinking').classList.toggle('active', busy);
  for (const id of ['new-game', 'load-fen', 'side']) $(id).disabled = !ready || changing;
  $('undo').disabled = !ready || changing || !moves.length;
  $('engine-move').disabled = !ready || changing || busy || selfPlay || game.isGameOver();
  $('self-play').disabled = !ready || changing || game.isGameOver();
  $('self-play').textContent = selfPlay ? 'Pause self-play' : 'Start self-play';
  $('self-play').setAttribute('aria-pressed', String(selfPlay));
  $('stop').disabled = !busy || changing;
  for (const [id, color] of [['bottom-player', orientation], ['top-player', orientation === 'w' ? 'b' : 'w']]) {
    $(id).textContent = `${selfPlay || color !== player ? 'Engine' : 'You'} · ${colorName(color)}`;
  }
  const history = game.history({ verbose: true });
  $('moves').replaceChildren();
  let row;
  for (const move of history) {
    const number = move.before.split(' ')[5];
    if (move.color === 'w' || !row) {
      row = document.createElement('li');
      for (const value of [number + '.', '…', '']) {
        const entry = document.createElement('span'); entry.textContent = value; row.append(entry);
      }
      row.firstChild.className = 'number';
      $('moves').append(row);
    }
    row.children[move.color === 'w' ? 1 : 2].textContent = move.san;
  }
  $('empty-moves').hidden = Boolean(history.length);
  $('moves').scrollTop = $('moves').scrollHeight;
  $('board').dataset.fen = game.fen();
  if (!fenDirty) $('fen').value = game.fen();
}

function clearAnalysis() {
  for (const id of ['score', 'depth', 'nodes']) $(id).textContent = '—';
  $('pv').textContent = "The engine's planned continuation appears here.";
}
function showInfo(line, searchFen) {
  const depth = line.match(/\bdepth (\d+)/);
  const nodes = line.match(/\bnodes (\d+)/);
  const score = line.match(/\bscore (cp|mate) (-?\d+)/);
  if (depth) $('depth').textContent = depth[1];
  if (nodes) $('nodes').textContent = Number(nodes[1]).toLocaleString();
  if (score) {
    const value = Number(score[2]) * (searchFen.split(' ')[1] === 'w' ? 1 : -1);
    $('score').textContent = score[1] === 'mate' ? `${value < 0 ? '−' : '+'}M${Math.abs(value)}` : `${value > 0 ? '+' : ''}${(value / 100).toFixed(2)}`;
  }
  const pv = line.match(/\bpv (.+)/);
  if (pv) {
    const cursor = new Chess(searchFen);
    const san = [];
    for (const uci of pv[1].trim().split(/\s+/)) {
      try { san.push(cursor.move({ from: uci.slice(0, 2), to: uci.slice(2, 4), promotion: uci[4] }).san); }
      catch { break; }
    }
    $('pv').textContent = san.join(' ') || pv[1];
  }
}

async function selectSquare(square) {
  if (!canMove()) return;
  if (selected === square) { selected = null; render(); return; }
  const options = selected ? game.moves({ square: selected, verbose: true }).filter(move => move.to === square) : [];
  if (!options.length) { selected = game.get(square)?.color === player ? square : null; render(); return; }
  const from = selected;
  let promotion;
  if (options.some(move => move.promotion)) {
    $('promotion').showModal();
    promotion = await new Promise(resolve => { promotionChoice = resolve; });
    if (!promotion) return;
  }
  if (!canMove()) return;
  const move = game.move({ from, to: square, promotion });
  moves.push(move.lan);
  lastMove = move;
  selected = null;
  clearError(); render();
  if (!game.isGameOver()) await makeEngineMove();
}

async function makeEngineMove() {
  if (!ready || busy || changing || game.isGameOver()) return;
  busy = true;
  const token = ++generation;
  const searchFen = game.fen();
  selected = null;
  clearError(); clearAnalysis(); render();
  try {
    await engine.position(positionCommand());
    if (token !== generation) return;
    const move = await engine.search(Number($('time').value), line => {
      if (token === generation) showInfo(line, searchFen);
    });
    if (token !== generation) return;
    if (!/^[a-h][1-8][a-h][1-8][qrbn]?$/.test(move)) throw new Error(`Engine returned no legal move (${move})`);
    const played = game.move({ from: move.slice(0, 2), to: move.slice(2, 4), promotion: move[4] });
    moves.push(played.lan);
    lastMove = played;
  } catch (error) { if (token === generation) { selfPlay = false; showError(error); } }
  finally {
    if (token === generation) {
      busy = false; render();
      if (selfPlay && !game.isGameOver()) autoTimer = setTimeout(() => void makeEngineMove(), 80);
    }
  }
}

async function changePosition(action) {
  if (!ready || changing) return;
  changing = true;
  selfPlay = false; clearTimeout(autoTimer);
  ++generation; // discard all progress and bestmove from the old position
  selected = null;
  render();
  try { await engine.stop(); await action(); clearError(); clearAnalysis(); }
  catch (error) { showError(error); }
  finally { changing = busy = false; render(); }
}

$('new-game').onclick = async () => {
  await changePosition(async () => {
    await engine.position('position startpos', true);
    game = new Chess(); rootFen = DEFAULT_POSITION; moves = []; lastMove = null;
    fenDirty = false;
  });
  if (player === 'b') void makeEngineMove();
};
$('load-fen').onclick = () => {
  let next;
  try { next = new Chess($('fen').value.trim().replace(/\s+/g, ' ')); }
  catch (error) { showError(new Error(`Invalid FEN: ${error.message}`)); return; }
  void changePosition(async () => {
    await engine.position(`position fen ${next.fen()}`, true);
    game = next; rootFen = next.fen(); moves = []; lastMove = null;
    fenDirty = false;
    $('fen').value = next.fen();
  });
};
$('undo').onclick = () => void changePosition(async () => {
  game.undo(); moves.pop();
  if (game.turn() !== player && moves.length) { game.undo(); moves.pop(); }
  lastMove = game.history({ verbose: true }).at(-1) || null;
  fenDirty = false;
  await engine.position(positionCommand());
});
$('side').onchange = async () => {
  const next = $('side').value;
  await changePosition(async () => { player = orientation = next; });
  if (game.turn() !== player) void makeEngineMove();
};
$('flip').onclick = () => { orientation = orientation === 'w' ? 'b' : 'w'; render(); };
$('fen').addEventListener('input', () => { fenDirty = true; });
$('engine-move').onclick = () => void makeEngineMove();
$('stop').onclick = () => {
  selfPlay = false; clearTimeout(autoTimer); render();
  void engine.stop().catch(showError); // accept last completed iteration
};
$('self-play').onclick = () => {
  selfPlay = !selfPlay; clearTimeout(autoTimer); render();
  if (selfPlay) void makeEngineMove();
  else void engine.stop().catch(showError);
};
for (const button of document.querySelectorAll('[data-promotion]')) button.onclick = () => {
  promotionChoice?.(button.dataset.promotion); promotionChoice = null; $('promotion').close();
};
function cancelPromotion() { promotionChoice?.(null); promotionChoice = null; $('promotion').close(); selected = null; render(); }
$('cancel-promotion').onclick = cancelPromotion;
$('promotion').addEventListener('cancel', event => { event.preventDefault(); cancelPromotion(); });
window.addEventListener('pagehide', () => engine.worker.terminate());
render();
$('fen').value = game.fen();
engine.ready.then(() => { ready = true; render(); }).catch(error => {
  $('status').textContent = 'Engine could not start. Reload the page to try again.'; showError(error);
});
