'use strict';

const PORT        = parseInt(process.env.PORT        || '3000', 10);
const MAX_HISTORY = parseInt(process.env.MAX_HISTORY || '50',   10);

// { ts: 'HH:MM:SS', raw: '<broadcast string>' }
const history = [];
const clients = new Set();

function nowTs() {
  return new Date().toTimeString().slice(0, 8);
}

function addHistory(raw) {
  history.push({ ts: nowTs(), raw });
  if (history.length > MAX_HISTORY) history.shift();
}

function broadcastCount() {
  const msg = `[COUNT]:${clients.size}`;
  for (const ws of clients) ws.send(msg);
}

Bun.serve({
  port: PORT,

  fetch(req, server) {
    if (server.upgrade(req)) return;
    return new Response('FalloutChat WebSocket server', { status: 200 });
  },

  websocket: {
    open(ws) {
      clients.add(ws);
      console.log(`[+] connected  total=${clients.size}`);

      for (const h of history) {
        ws.send(`[HISTORY]${h.ts}|${h.raw}`);
      }

      broadcastCount();
    },

    message(ws, data) {
      const msg = String(data);

      if (msg.startsWith('[RENAME]')) {
        console.log(`[rename] ${msg.slice(8)}`);
        return;
      }

      let formatted;

      if (msg.startsWith('[EMOTE]')) {
        formatted = msg;
      } else {
        const ci = msg.indexOf(': ');
        if (ci === -1) {
          console.warn(`[bad msg] no ': ' delimiter: ${msg.slice(0, 80)}`);
          return;
        }

        const prefix   = msg.slice(0, ci);
        const text     = msg.slice(ci + 2).trim();
        if (!text) return;

        const parts    = prefix.split('|');
        const steamId  = parts[0] ?? '0';
        const username = (parts[1] ?? 'Player').trim();

        const display  = steamId === '0' ? `[Web] ${username}` : username;

        formatted = text.startsWith('/me ')
          ? `[EMOTE]${display}\x01${text.slice(4)}`
          : `${display}: ${text}`;
      }

      if (formatted.startsWith('[EMOTE]')) {
        const d = formatted.indexOf('\x01');
        console.log(`[emote] * ${formatted.slice(7, d)} ${formatted.slice(d + 1)}`);
      } else {
        const d = formatted.indexOf(': ');
        if (d !== -1) console.log(`[chat]  ${formatted.slice(0, d)}: ${formatted.slice(d + 2)}`);
      }

      addHistory(formatted);

      for (const client of clients) {
        if (client !== ws) client.send(formatted);
      }
    },

    close(ws) {
      clients.delete(ws);
      console.log(`[-] disconnected  total=${clients.size}`);
      broadcastCount();
    },

    error(ws, err) {
      console.error(`[ws error] ${err.message}`);
      clients.delete(ws);
    },
  },
});

console.log(`FalloutChat server  port=${PORT}  max_history=${MAX_HISTORY}`);
