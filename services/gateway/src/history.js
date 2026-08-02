export class ChannelHistory {
  constructor({ limit = 100, replayLimit = 50, channelLimit = 200 } = {}) {
    this.limit = Math.max(1, limit);
    this.replayLimit = Math.min(this.limit, Math.max(1, replayLimit));
    this.channelLimit = Math.max(1, channelLimit);
    this.channels = new Map();
  }

  add(channel, message) {
    if (!channel) return;
    // Channel names are cheap to invent, so without a cap on how many of them
    // are remembered, anyone can grow this map until the process runs out of
    // memory. Map iterates in insertion order, so the oldest channel is the
    // first key and dropping it gives least-recently-created eviction.
    if (!this.channels.has(channel) && this.channels.size >= this.channelLimit) {
      this.channels.delete(this.channels.keys().next().value);
    }
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
