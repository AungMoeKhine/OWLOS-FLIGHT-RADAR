// a test only, never on the site: boards behind a pretend navigator.serial, speaking the console's lines.
// assemble.sh --test puts it in _public/test.html
(() => {
  const BOARDS = [
    { mac: 'aa0000000002', build: 'ili9341', theme: 2 },
    { mac: 'aa0000000001', build: 'st7796', theme: 0 },
    { mac: 'aa0000000003', build: 'gc9a01', theme: 1 },
  ];
  class FakePort extends EventTarget {
    constructor(b) {
      super();
      this.b = b;
      this.text = '';
    }

    getInfo() {
      return { usbVendorId: 0x1a86 };
    }

    async open() {
      this.readable = new ReadableStream({
        start: (c) => {
          this.out = c;
        },
      });
      this.writable = new WritableStream({
        write: (chunk) => this.heard(new TextDecoder().decode(chunk)),
      });
    }

    async setSignals() {
    }

    async close() {
      clearInterval(this.sky);
      this.out = null;
    }

    line(text) {
      this.out?.enqueue(new TextEncoder().encode(`${text}\r\n`));
    }

    heard(text) {
      this.text += text;
      let i;
      while ((i = this.text.indexOf('\n')) >= 0) {
        const l = this.text.slice(0, i).trim();
        this.text = this.text.slice(i + 1);
        if (l === 'OWLOS HELLO') {
          this.line(`OWLOS READY flight-radar 0.8 ${this.b.build} ${this.b.mac}`);
        } else if (l === 'OWLOS GET') {
          this.line(`OWLOS CFG ${JSON.stringify({
            ssid: 'test-net', nets: ['test-net'], lat: 50.9, lon: 4.48, place: 'Testville Sčín', country: 'XX', up: 0, theme: this.b.theme, look: 0, map: 1, ground: 1, range: 50, dist_unit: 0,
            alt_unit: 0, labels: 1, vectors: 0, beam_step: 0, clock: 0, block: 0, autopilot: 0, lost: 0, conflict: 0, fl_lo: 0, fl_hi: 0, version: '0.8', screen: this.b.screen ?? 1, flip: this.b.flip ?? 0,
          })}`);
        } else if (l === 'OWLOS SKIP') {
          this.line('OWLOS ALIGN 0');
        } else if (l === 'OWLOS SKY ON') {
          clearInterval(this.sky);
          this.line(`OWLOS ALIGN ${this.b.build === 'gc9a01' ? 1 : 0}`);
          const answer = () => {
            this.line('OWLOS SKY BEGIN TEST');
            for (let k = 0; k < 3; k++) {
              this.line(`OWLOS PLANE {"hex":"abc00${k}","flight":"TST${k}","lat":${50.9 + k * 0.1},"lon":${4.48 + k * 0.1},"alt_baro":${20000 + k * 5000},"gs":400,"track":${k * 90}}`);
            }
            this.line('OWLOS SKY END 3');
          };
          answer();
          this.sky = setInterval(answer, 15000);
        } else if (l.startsWith('OWLOS SET ')) {
          const set = JSON.parse(l.slice(10));
          if (set.screen !== undefined) {
            this.b.screen = set.screen;
          }
          if (set.flip !== undefined) {
            this.b.flip = set.flip;
          }
          this.line('OWLOS OK');
        }
      }
    }
  }
  const ports = BOARDS.map((b) => new FakePort(b));
  let given = 0;
  const fake = new EventTarget();
  fake.requestPort = async () => ports[given++ % ports.length];
  fake.getPorts = async () => [];
  Object.defineProperty(navigator, 'serial', { value: fake, configurable: true });
})();
