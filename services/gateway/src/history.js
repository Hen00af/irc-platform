export class ChannelHistory {
  constructor({ limit = 100, replayLimit = 50 } = {}) {
    this.limit = Math.max(1, limit);
    this.replayLimit = Math.min(this.limit, Math.max(1, replayLimit));
    this.channels = new Map();
  }

  add(channel, message) {
    if (!channel) return;
    const messages = this.channels.get(channel) || [];
    messages.push({
      nick: message.nick,
      text: message.text,
      timestamp: message.timestamp || new Date().toISOString()
    });
    if (messages.length > this.limit) {
      messages.splice(0, messages.length - this.limit);
    }
    this.channels.set(channel, messages);
  }

  recent(channel) {
    return (this.channels.get(channel) || []).slice(-this.replayLimit);
  }
}
