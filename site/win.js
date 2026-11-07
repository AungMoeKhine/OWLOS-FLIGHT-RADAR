// Windows on the page: dragged by the title, on top when touched, with the usual three buttons.
// They start like tiles under the picture, in the middle of the page, and the page grows down for
// them. One that was moved stays where it was put until ARRANGE or the next visit. On a phone they
// simply follow each other, as a page does.

const GAP = 14;
const ROW = 1180;                                      // how wide the tiles may lie

class Win {
  constructor(desk, id, title, { onFront = () => {}, onClose = () => {}, onResize = () => {}, className = '' }) {
    this.desk = desk;
    this.id = id;
    this.onFront = onFront;
    this.onClose = onClose;
    this.onResize = onResize;
    this.root = document.createElement('section');
    this.root.className = `win ${className}`;
    this.root.hidden = true;
    this.root.innerHTML = `<header class="winbar"><span class="wintitle"></span>
      <button class="winbtn" data-act="min" title="Minimize" aria-label="minimize">&#8211;</button>
      <button class="winbtn" data-act="max" title="Maximize" aria-label="maximize">&#9633;</button>
      <button class="winbtn close" data-act="close" title="Close" aria-label="close">&#215;</button></header><div class="winbody"></div>`;
    this.bar = this.root.querySelector('.winbar');
    this.body = this.root.querySelector('.winbody');
    this.setTitle(title);
    this.at = null;                                    // { x, y } once it was moved
    this.listen();
    desk.el.append(this.root);
    new ResizeObserver(() => desk.layout()).observe(this.root);
  }

  q(selector) {
    return this.body.querySelector(selector);
  }

  setTitle(text) {
    this.root.querySelector('.wintitle').textContent = text;
  }

  listen() {
    this.root.addEventListener('pointerdown', () => this.front());
    this.bar.addEventListener('click', (e) => {
      const act = e.target.closest('[data-act]')?.dataset.act;
      if (act === 'max') {
        this.toggleMax();
      } else if (act) {
        this.hide();
        this.onClose(act);
      }
    });
    this.bar.addEventListener('dblclick', (e) => {
      if (!e.target.closest('[data-act]')) {
        this.toggleMax();
      }
    });
    this.bar.addEventListener('pointerdown', (e) => {
      if (e.target.closest('[data-act]') || this.maxed || this.desk.flat()) {
        return;
      }
      const startX = e.clientX - this.root.offsetLeft;
      const startY = e.clientY - this.root.offsetTop;
      this.bar.setPointerCapture(e.pointerId);
      const move = (m) => {
        this.at = { x: m.clientX - startX, y: m.clientY - startY };
        this.place();
        this.desk.layout();
      };
      const up = () => {
        this.bar.removeEventListener('pointermove', move);
        this.bar.removeEventListener('pointerup', up);
        this.bar.removeEventListener('pointercancel', up);
      };
      this.bar.addEventListener('pointermove', move);
      this.bar.addEventListener('pointerup', up);
      this.bar.addEventListener('pointercancel', up);
    });
  }

  place() {
    if (this.at && !this.desk.flat()) {
      const top = -this.desk.el.getBoundingClientRect().top - scrollY;
      this.root.style.left = `${Math.max(-this.root.offsetWidth + 90, Math.min(this.at.x, this.desk.el.clientWidth - 90))}px`;
      this.root.style.top = `${Math.max(top, this.at.y)}px`;
    }
  }

  show() {
    this.root.hidden = false;
    this.desk.layout();
    return this;
  }

  hide() {
    this.root.hidden = true;
    if (this.maxed) {
      this.toggleMax();
    }
    this.desk.layout();
  }

  front() {
    this.root.style.zIndex = String(++this.desk.z);
    this.onFront();
  }

  toggleMax() {
    this.maxed = !this.maxed;
    this.root.classList.toggle('maxed', this.maxed);
    this.onResize();
    this.desk.layout();
  }
}

export class Desk {
  constructor(el) {
    this.el = el;
    this.z = 10;
    this.wins = new Map();
    addEventListener('resize', () => this.layout());
  }

  flat() {
    return innerWidth < 720;
  }

  get(id) {
    return this.wins.get(id);
  }

  make(id, title, options = {}) {
    const w = new Win(this, id, title, options);
    this.wins.set(id, w);
    return w;
  }

  remove(id) {
    this.wins.get(id)?.root.remove();
    this.wins.delete(id);
    this.layout();
  }

  /* the highest free place, then the leftmost, for a window of this size, among those already lying */
  spot(w, h, lying) {
    const width = Math.min(ROW, this.el.clientWidth);
    const xs = [0, ...lying.map((r) => r.x + r.w + GAP)].filter((x) => x === 0 || x + w <= width).sort((a, b) => a - b);
    const ys = [0, ...lying.map((r) => r.y + r.h + GAP)].sort((a, b) => a - b);
    for (const y of ys) {
      for (const x of xs) {
        const free = lying.every((r) => x + w + GAP <= r.x || r.x + r.w + GAP <= x || y + h + GAP <= r.y || r.y + r.h + GAP <= y);
        if (free) {
          return { x, y };
        }
      }
    }
    return { x: 0, y: Math.max(0, ...lying.map((r) => r.y + r.h + GAP)) };
  }

  /* those never moved are laid like tiles round those that were. the page reaches past the lowest one */
  layout() {
    if (this.flat()) {
      this.el.style.height = '';
      return;
    }
    const shown = [...this.wins.values()].filter((w) => !w.root.hidden && !w.maxed);
    const tiles = [];
    for (const w of shown.filter((v) => !v.at)) {
      const r = w.root;
      const { x, y } = this.spot(r.offsetWidth, r.offsetHeight, tiles);
      tiles.push({ r, x, y, w: r.offsetWidth, h: r.offsetHeight });
    }
    // the tiles as one block in the middle of the page
    const used = Math.max(0, ...tiles.map((t) => t.x + t.w));
    const left = Math.max(0, (this.el.clientWidth - used) / 2);
    for (const t of tiles) {
      t.r.style.left = `${left + t.x}px`;
      t.r.style.top = `${t.y}px`;
    }
    let bottom = Math.max(0, ...tiles.map((t) => t.y + t.h));
    for (const w of shown.filter((v) => v.at)) {
      w.place();
      bottom = Math.max(bottom, w.root.offsetTop + w.root.offsetHeight);
    }
    this.el.style.height = `${bottom + GAP}px`;
  }

  /* everything back in its tiles */
  arrange() {
    for (const w of this.wins.values()) {
      w.at = null;
      if (w.maxed) {
        w.toggleMax();
      }
    }
    this.layout();
  }
}
