import { LineSplitter } from "./lines.js";

// USB vendor IDs for Arduino boards: Arduino LLC (0x2341) and Arduino SRL (0x2A03).
const ARDUINO_FILTERS = [{ usbVendorId: 0x2341 }, { usbVendorId: 0x2a03 }];
// Keep the page fast: drop the oldest lines past this many.
const MAX_LINES = 5000;

const $ = (id) => document.getElementById(id);
const ui = {
  status: $("status"),
  statusText: $("status-text"),
  baud: $("baud"),
  anyDevice: $("any-device"),
  connect: $("connect"),
  clear: $("clear"),
  log: $("log"),
  unsupported: $("unsupported"),
  send: $("send"),
  sendText: $("send-text"),
  sendEnding: $("send-ending"),
};

let port = null;
let reader = null;
let reading = null; // the promise of the read loop, so disconnect can wait for it
let currentLine = null; // the line being added to, until its line ending arrives

// --- setup -----------------------------------------------------------------

// `task monitor` passes the firmware's baud rate as ?baud=…
const baud = new URLSearchParams(location.search).get("baud") ?? "9600";
if (![...ui.baud.options].some((o) => o.value === baud)) ui.baud.add(new Option(baud));
ui.baud.value = baud;

if (!("serial" in navigator)) {
  ui.unsupported.hidden = false;
  ui.connect.disabled = true;
} else {
  navigator.serial.addEventListener("disconnect", (event) => {
    if (event.target === port) {
      note("Board unplugged");
      disconnect();
    }
  });
}

ui.connect.addEventListener("click", () => (port ? disconnect() : connect()));
ui.clear.addEventListener("click", clearLog);
ui.send.addEventListener("submit", (event) => {
  event.preventDefault();
  send();
});

// --- connection ------------------------------------------------------------

async function connect() {
  try {
    const filters = ui.anyDevice.checked ? [] : ARDUINO_FILTERS;
    port = await navigator.serial.requestPort({ filters });
  } catch {
    return; // the picker was cancelled
  }

  try {
    await port.open({ baudRate: Number(ui.baud.value) });
  } catch (error) {
    port = null;
    note(`Couldn't open the port: ${error.message}. Is another program using it?`, "error");
    return;
  }

  setConnected(true);
  note(`Connected at ${ui.baud.value} baud`);
  reading = readLoop();
}

async function disconnect() {
  const closing = port;
  if (!closing) return;
  port = null;
  setConnected(false);

  try {
    await reader?.cancel(); // ends the read loop
    await reading;
    await closing.close();
  } catch {
    // Already gone, e.g. unplugged: nothing left to close.
  }
  note("Disconnected");
}

async function readLoop() {
  const decoder = new TextDecoder(); // streaming, so multi-byte characters can span reads
  const splitter = new LineSplitter();

  while (port?.readable) {
    reader = port.readable.getReader();
    try {
      for (;;) {
        const { value, done } = await reader.read();
        if (done) break;
        for (const piece of splitter.push(decoder.decode(value, { stream: true }))) {
          appendText(piece.text);
          if (piece.ends) currentLine = null;
        }
      }
    } catch (error) {
      // Framing or parity errors (often a baud mismatch) land here; keep reading.
      if (port) note(`Read error: ${error.message}`, "error");
    } finally {
      reader.releaseLock();
      reader = null;
    }
  }
  currentLine = null;
}

async function send() {
  const text = ui.sendText.value + ui.sendEnding.value;
  if (!port?.writable || text === "") return;

  const writer = port.writable.getWriter();
  try {
    await writer.write(new TextEncoder().encode(text));
    ui.sendText.value = "";
  } finally {
    writer.releaseLock();
  }
}

function setConnected(connected) {
  ui.status.dataset.state = connected ? "connected" : "idle";
  ui.statusText.textContent = connected ? `Connected · ${ui.baud.value} baud` : "Disconnected";
  ui.connect.textContent = connected ? "Disconnect" : "Connect";
  ui.baud.disabled = connected;
  ui.anyDevice.disabled = connected;
  for (const el of [ui.sendText, ui.sendEnding, ui.send.querySelector("button")]) {
    el.disabled = !connected;
  }
  if (connected) ui.sendText.focus();
}

// --- log -------------------------------------------------------------------

function appendText(text) {
  const follow = isScrolledToBottom();

  if (!currentLine) {
    ui.log.querySelector(".empty")?.remove();
    currentLine = newLine("line");
  }
  if (text) currentLine.lastChild.append(render(text));

  trimLog();
  if (follow) scrollToBottom();
}

// A message from the monitor itself, not the board.
function note(text, kind = "info") {
  const follow = isScrolledToBottom();
  ui.log.querySelector(".empty")?.remove();
  currentLine = null;
  newLine(`note ${kind}`).lastChild.textContent = text;
  if (follow) scrollToBottom();
}

function newLine(className) {
  const line = document.createElement("div");
  line.className = className;

  const time = document.createElement("time");
  time.textContent = new Date().toLocaleTimeString([], {
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
    fractionalSecondDigits: 3,
    hour12: false,
  });

  line.append(time, document.createElement("span"));
  ui.log.append(line);
  return line;
}

// Show characters a terminal wouldn't print (NUL, other control codes, and
// bytes that aren't valid text, often from a baud mismatch) as visible markers.
function render(text) {
  const fragment = document.createDocumentFragment();
  for (const part of text.split(/([\x00-\x08\x0b-\x1f\x7f�])/)) {
    if (!part) continue;
    if (/^[\x00-\x08\x0b-\x1f\x7f�]$/.test(part)) {
      const mark = document.createElement("span");
      mark.className = "control";
      const code = part.codePointAt(0);
      mark.textContent = code === 0xfffd ? "�" : `\\x${code.toString(16).padStart(2, "0")}`;
      fragment.append(mark);
    } else {
      fragment.append(part);
    }
  }
  return fragment;
}

function trimLog() {
  while (ui.log.childElementCount > MAX_LINES) ui.log.firstElementChild.remove();
}

function clearLog() {
  ui.log.replaceChildren();
  currentLine = null;
}

function isScrolledToBottom() {
  return ui.log.scrollHeight - ui.log.scrollTop - ui.log.clientHeight < 24;
}

function scrollToBottom() {
  ui.log.scrollTop = ui.log.scrollHeight;
}
