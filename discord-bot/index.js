import { Client, GatewayIntentBits, Events } from 'discord.js';

const TOKEN      = process.env.DISCORD_TOKEN;
const CHANNEL_ID = process.env.DISCORD_CHANNEL_ID;
const WS_URL     = process.env.WS_URL     || 'ws://localhost:3000';
const BOT_SECRET = process.env.BOT_SECRET || '';

if (!TOKEN || !CHANNEL_ID) {
  console.error('[bot] Missing DISCORD_TOKEN or DISCORD_CHANNEL_ID');
  process.exit(1);
}

// ── Discord ───────────────────────────────────────────────────────────────
// Requires "Message Content" privileged intent enabled in Discord Dev Portal
const discord = new Client({
  intents: [
    GatewayIntentBits.Guilds,
    GatewayIntentBits.GuildMessages,
    GatewayIntentBits.MessageContent,
  ],
});

let channel      = null;
let ws           = null;
let wsReady      = false;
let reconnecting = false;

// ── Helpers ───────────────────────────────────────────────────────────────
function escMd(s) {
  return String(s).replace(/([*_`~\\|])/g, '\\$1');
}

// ── WebSocket → Discord ───────────────────────────────────────────────────
function connectWS() {
  if (reconnecting) return;
  reconnecting = true;

  ws = new WebSocket(WS_URL);

  ws.onopen = () => {
    wsReady      = true;
    reconnecting = false;
    console.log('[ws] connected to chat server');
    if (BOT_SECRET) ws.send(`[BOT_AUTH]${BOT_SECRET}`);
  };

  ws.onmessage = ({ data }) => {
    // Ignore protocol frames and history on connect
    if (data.startsWith('[COUNT]:') || data.startsWith('[HISTORY]')) return;
    if (!channel) return;

    let text;
    if (data.startsWith('[EMOTE]')) {
      const d      = data.indexOf('\x01');
      const sender = d !== -1 ? data.slice(7, d)  : 'Someone';
      const action = d !== -1 ? data.slice(d + 1) : '';
      text = `_\\* **${escMd(sender)}** ${escMd(action)}_`;
    } else {
      const ci = data.indexOf(': ');
      if (ci === -1) return;
      text = `**${escMd(data.slice(0, ci))}**: ${escMd(data.slice(ci + 2))}`;
    }

    channel.send(text).catch(err => console.error('[discord] send error:', err));
  };

  ws.onclose = () => {
    wsReady      = false;
    reconnecting = false;
    console.log('[ws] disconnected — reconnecting in 5s');
    setTimeout(connectWS, 5000);
  };

  ws.onerror = () => ws.close();
}

// ── Discord → WebSocket ───────────────────────────────────────────────────
discord.on(Events.MessageCreate, msg => {
  if (msg.author.bot)              return; // ignore bots (including itself)
  if (msg.channelId !== CHANNEL_ID) return;
  if (!msg.content.trim())         return;
  if (!wsReady || ws?.readyState !== WebSocket.OPEN) {
    console.warn('[discord] message received but WS not ready');
    return;
  }

  const name    = msg.member?.displayName ?? msg.author.username;
  const content = msg.content.slice(0, 500);

  // Format: 0|[Discord] Name|Discord: text
  // Server trusts this because BOT_AUTH was sent on connect
  ws.send(`0|[Discord] ${name}|Discord: ${content}`);
});

// ── Boot ──────────────────────────────────────────────────────────────────
discord.once(Events.ClientReady, async () => {
  console.log(`[discord] logged in as ${discord.user.tag}`);

  channel = await discord.channels.fetch(CHANNEL_ID).catch(() => null);
  if (!channel) {
    console.error(`[discord] channel ${CHANNEL_ID} not found — check DISCORD_CHANNEL_ID`);
    process.exit(1);
  }
  console.log(`[discord] relaying #${channel.name} ↔ game chat`);
});

connectWS();
discord.login(TOKEN);
