// One board on one cable: the lines of firmware/main/console.h over Web Serial.

const within = (promise, ms) => Promise.race([promise, new Promise((r) => {
  setTimeout(r, ms);
})]);

/* the link on each port: a port left open by a try that failed must be let go, or it stays busy */
const links = new WeakMap();

export class Link {
  constructor(port) {
    this.port = port;
    this.encoder = new TextEncoder();
    this.lines = [];
    this.writing = Promise.resolve();
    this.onLine = null;                                // every line, the log too
    this.onLost = null;
  }

  async open() {
    await links.get(this.port)?.close();
    links.set(this.port, this);
    await this.port.open({ baudRate: 115200, bufferSize: 65536 });
    // both lines released: the board runs. on the CYD this may restart it, it then says READY anyway
    await this.port.setSignals({ dataTerminalReady: false, requestToSend: false });
    this.reader = this.port.readable.getReader();
    this.pumping = this.pump();
  }

  async pump() {
    const decoder = new TextDecoder();
    let text = '';
    try {
      for (;;) {
        const { value, done } = await this.reader.read();
        if (done) {
          break;
        }
        text += decoder.decode(value, { stream: true });
        let i;
        while ((i = text.indexOf('\n')) >= 0) {
          const line = text.slice(0, i).replace(/\r$/, '').replace(/\x1b\[[0-9;]*m/g, '');
          text = text.slice(i + 1);
          this.take(line);
        }
      }
    } catch (e) {
      console.warn('the port went away', e);
    }
    this.onLost?.();
  }

  take(line) {
    if (line.startsWith('OWLOS ') && !line.startsWith('OWLOS PLANE') && !line.startsWith('OWLOS SKY')) {
      this.lines.push(line);
      if (this.lines.length > 64) {
        this.lines.shift();
      }
    }
    this.onLine?.(line);
  }

  // a newline first: bytes garbled while the board restarts would otherwise glue onto the command.
  // one at a time: the port has one writer
  send(line) {
    this.writing = this.writing.then(async () => {
      const writer = this.port.writable.getWriter();
      await writer.write(this.encoder.encode(`\n${line}\n`));
      writer.releaseLock();
    });
    return this.writing;
  }

  async expect(prefix, ms) {
    const until = Date.now() + ms;
    while (Date.now() < until) {
      const i = this.lines.findIndex((l) => l.startsWith(prefix));
      if (i >= 0) {
        return this.lines.splice(i, 1)[0];
      }
      await new Promise((r) => setTimeout(r, 60));
    }
    return null;
  }

  /* who is on the cable: { version, build, mac }, build and mac only from 0.8 on. null: no Flight Radar */
  async hello(tries = 8) {
    for (let t = 0; t < tries; t++) {
      await this.send('OWLOS HELLO');
      const line = await this.expect('OWLOS READY', 800);
      if (line) {
        const [, , what, version, build, mac] = line.split(' ');
        return what === 'flight-radar' ? { version, build, mac } : null;
      }
    }
    return null;
  }

  async config() {
    await this.send('OWLOS GET');
    const line = await this.expect('OWLOS CFG ', 3000);
    return line ? JSON.parse(line.slice(10)) : null;
  }

  async close() {
    this.onLost = null;
    this.onLine = null;
    clearInterval(this.keep);
    links.delete(this.port);
    if (!this.reader) {
      return;
    }
    try {
      await within(this.send('OWLOS SKY OFF'), 800);   // a board that does not read must not hold the port
    } catch (e) {
      console.warn(e);
    }
    try {
      await within(this.reader.cancel(), 1000);
      await within(this.pumping, 1000);
      this.reader.releaseLock();
      await this.port.close();
    } catch (e) {
      console.warn(e);
    }
  }
}
