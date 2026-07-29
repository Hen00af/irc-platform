const state = {
  socket: null,
  nickname: "",
  channel: "#lobby",
  connected: false,
  members: new Set()
};

const elements = {
  route: document.querySelector(".route"),
  hops: [...document.querySelectorAll("[data-hop]")],
  connection: document.querySelector(".connection-copy"),
  connectionLabel: document.querySelector("#connection-label"),
  channelTitle: document.querySelector("#channel-title"),
  avatarLetter: document.querySelector("#avatar-letter"),
  messages: document.querySelector("#messages"),
  members: document.querySelector("#members"),
  memberCount: document.querySelector("#member-count"),
  composer: document.querySelector("#composer"),
  input: document.querySelector("#message-input"),
  send: document.querySelector("#composer button"),
  changeChannel: document.querySelector("#change-channel"),
  changeNick: document.querySelector("#change-nick"),
  dialog: document.querySelector("#connect-dialog"),
  connectForm: document.querySelector("#connect-form"),
  nickname: document.querySelector("#nickname"),
  channel: document.querySelector("#channel"),
  connectError: document.querySelector("#connect-error")
};

function gatewayUrl() {
  const override = new URLSearchParams(location.search).get("gateway");
  if (override) return override;
  const local = ["localhost", "127.0.0.1", "::1"].includes(location.hostname);
  const protocol = location.protocol === "https:" ? "wss:" : "ws:";
  return `${protocol}//${location.hostname}:${local ? "3001" : "8443"}`;
}

function setConnection(level, label) {
  elements.connectionLabel.textContent = label;
  elements.connection.classList.toggle("is-online", level === 3);
  elements.route.classList.toggle("is-moving", level > 1);
  elements.hops.forEach((hop, index) => {
    hop.classList.toggle("is-live", index < level);
  });
}

function timestamp(value) {
  return new Intl.DateTimeFormat([], {
    hour: "2-digit",
    minute: "2-digit"
  }).format(value ? new Date(value) : new Date());
}

function addMessage({ nick, text, own = false, timestamp: sentAt }) {
  if (!text) return;
  const item = document.createElement("li");
  item.className = `message${own ? " own" : ""}`;

  const avatar = document.createElement("span");
  avatar.className = "message-avatar";
  avatar.textContent = (nick || "?").slice(0, 1).toUpperCase();

  const content = document.createElement("div");
  const head = document.createElement("div");
  head.className = "message-head";
  const name = document.createElement("strong");
  name.textContent = nick || "server";
  const time = document.createElement("time");
  time.textContent = timestamp(sentAt);
  const body = document.createElement("p");
  body.className = "message-body";
  body.textContent = text;

  head.append(name, time);
  content.append(head, body);
  item.append(avatar, content);
  elements.messages.append(item);
  elements.messages.scrollTop = elements.messages.scrollHeight;
}

function addSystem(text, accent = "") {
  const item = document.createElement("li");
  item.className = "system-message";
  const label = document.createElement("span");
  label.textContent = accent;
  item.append(label, document.createTextNode(`${accent ? " " : ""}${text}`));
  elements.messages.append(item);
  elements.messages.scrollTop = elements.messages.scrollHeight;
}

function renderMembers() {
  const names = [...state.members].sort((a, b) => a.localeCompare(b));
  elements.members.replaceChildren(
    ...names.map((nickname) => {
      const item = document.createElement("li");
      item.className = "member";
      item.textContent = nickname;
      return item;
    })
  );
  elements.memberCount.textContent = String(names.length);
}

function send(payload) {
  if (!state.socket || state.socket.readyState !== WebSocket.OPEN) return;
  state.socket.send(JSON.stringify(payload));
}

function onGatewayMessage(message) {
  switch (message.type) {
    case "hello":
      setConnection(2, "Gateway linked");
      send({
        type: "connect",
        nickname: state.nickname,
        channel: state.channel
      });
      break;
    case "connecting":
      setConnection(2, "Opening IRC");
      break;
    case "ready":
      state.connected = true;
      setConnection(3, "Live");
      elements.input.disabled = false;
      elements.send.disabled = false;
      elements.input.focus();
      elements.dialog.close();
      addSystem(`Connected as ${state.nickname}`, "IRC");
      break;
    case "message":
      addMessage(message);
      if (message.nick) state.members.add(message.nick);
      renderMembers();
      break;
    case "history":
      if (message.channel !== state.channel) break;
      if (message.messages.length) {
        addSystem(
          `Showing ${message.messages.length} recent in-memory messages`,
          "HISTORY"
        );
        message.messages.forEach(addMessage);
      } else {
        addSystem("No recent messages in this channel", "HISTORY");
      }
      addSystem("History is cleared whenever the server restarts", "MEMORY");
      break;
    case "join":
      if (message.nick) state.members.add(message.nick);
      addSystem(`${message.nick || "A user"} joined ${message.channel}`, "JOIN");
      renderMembers();
      break;
    case "part":
      state.members.delete(message.nick);
      addSystem(`${message.nick || "A user"} left ${message.channel}`, "PART");
      renderMembers();
      break;
    case "kick":
      state.members.delete(message.target);
      addSystem(`${message.target} was removed from ${message.channel}`, "KICK");
      renderMembers();
      break;
    case "nick":
      if (state.members.delete(message.nick)) {
        state.members.add(message.nickname);
      }
      if (message.nick === state.nickname) {
        state.nickname = message.nickname;
        updateIdentity();
      }
      addSystem(`${message.nick} is now ${message.nickname}`, "NICK");
      renderMembers();
      break;
    case "who":
      if (message.nickname) state.members.add(message.nickname);
      renderMembers();
      break;
    case "error":
      addSystem(message.message || message.text || "IRC command failed", "ERROR");
      elements.connectError.textContent =
        message.message || message.text || "Could not connect";
      break;
    case "disconnected":
      disconnectUi("IRC disconnected");
      break;
  }
}

function connect() {
  if (state.socket) state.socket.close();
  setConnection(1, "Opening gateway");
  elements.connectError.textContent = "";
  const socket = new WebSocket(gatewayUrl());
  state.socket = socket;
  socket.addEventListener("message", (event) => {
    try {
      onGatewayMessage(JSON.parse(event.data));
    } catch {
      addSystem("Gateway sent an unreadable response", "ERROR");
    }
  });
  socket.addEventListener("error", () => {
    elements.connectError.textContent =
      "The chat gateway is unavailable. Check the deployment and try again.";
    disconnectUi("Gateway unavailable");
  });
  socket.addEventListener("close", () => {
    if (state.connected) addSystem("Connection closed", "OFFLINE");
    disconnectUi("Offline");
  });
}

function disconnectUi(label) {
  state.connected = false;
  setConnection(1, label);
  elements.input.disabled = true;
  elements.send.disabled = true;
}

function updateIdentity() {
  elements.avatarLetter.textContent =
    state.nickname.slice(0, 1).toUpperCase() || "?";
  localStorage.setItem("relay.nickname", state.nickname);
}

elements.connectForm.addEventListener("submit", (event) => {
  event.preventDefault();
  if (!elements.connectForm.reportValidity()) return;
  state.nickname = elements.nickname.value.trim();
  state.channel = elements.channel.value.trim();
  state.members.clear();
  renderMembers();
  updateIdentity();
  elements.channelTitle.textContent = state.channel;
  elements.input.placeholder = `Write to ${state.channel}`;
  connect();
});

elements.composer.addEventListener("submit", (event) => {
  event.preventDefault();
  const text = elements.input.value.trim();
  if (!text || !state.connected) return;
  send({ type: "message", target: state.channel, text });
  elements.input.value = "";
  elements.input.style.height = "";
});

elements.input.addEventListener("keydown", (event) => {
  if (event.key === "Enter" && !event.shiftKey) {
    event.preventDefault();
    elements.composer.requestSubmit();
  }
});

elements.input.addEventListener("input", () => {
  elements.input.style.height = "auto";
  elements.input.style.height = `${Math.min(elements.input.scrollHeight, 130)}px`;
});

elements.changeChannel.addEventListener("click", () => {
  const next = prompt("Join a channel", state.channel);
  if (!next || next === state.channel) return;
  send({ type: "part", channel: state.channel });
  state.channel = next.trim();
  state.members.clear();
  renderMembers();
  elements.channelTitle.textContent = state.channel;
  elements.input.placeholder = `Write to ${state.channel}`;
  send({ type: "join", channel: state.channel });
});

elements.changeNick.addEventListener("click", () => {
  const next = prompt("Choose a new nickname", state.nickname);
  if (!next || next === state.nickname) return;
  send({ type: "nick", nickname: next.trim() });
});

elements.nickname.value = localStorage.getItem("relay.nickname") || "";
elements.dialog.showModal();
