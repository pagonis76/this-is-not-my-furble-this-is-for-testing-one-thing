// Furby Connect over Web Bluetooth, for Furby Theatre.
// Connect, send actions and antenna colours, and upload + activate a DLC.
// The protocol is the same one FurBLE (furble.js) uses.
(() => {
  const SERVICE = 'dab91435-b5a1-e29c-b041-bcd562613bde';
  const UUIDS = {
    'dab91382-b5a1-e29c-b041-bcd562613bde': 'GPListen',
    'dab91383-b5a1-e29c-b041-bcd562613bde': 'GPWrite',
    'dab90756-b5a1-e29c-b041-bcd562613bde': 'NordicListen',
    'dab90757-b5a1-e29c-b041-bcd562613bde': 'NordicWrite',
    'dab90758-b5a1-e29c-b041-bcd562613bde': 'FileWrite',
  };
  const SLOT_EMPTY = 0, SLOT_FILLED = 2, SLOT_ACTIVE = 3;
  const sleep = ms => new Promise(r => setTimeout(r, ms));
  const hex = a => Array.from(a instanceof DataView ? new Uint8Array(a.buffer, a.byteOffset, a.byteLength) : a)
    .map(b => b.toString(16).padStart(2, '0')).join('');

  class FurbyBLE {
    constructor(log) {
      this.log = log || (() => {});
      this.device = null; this.c = {}; this.connected = false;
      this.listeners = new Set(); this.nordic = null;
      this.lastSend = 0; this.transferring = false; this.keepAlive = null;
      this.onchange = null;
    }

    async connect() {
      this.device = await navigator.bluetooth.requestDevice({ filters: [{ name: 'Furby' }], optionalServices: [SERVICE] });
      this.device.addEventListener('gattserverdisconnected', () => {
        this.connected = false; clearInterval(this.keepAlive);
        this.log('Furby disconnected. If this keeps happening, the batteries may be low.');
        this.onchange && this.onchange();
      });
      const server = await this.device.gatt.connect();
      const service = await server.getPrimaryService(SERVICE);
      for (const ch of await service.getCharacteristics()) {
        const n = UUIDS[ch.uuid]; if (n) this.c[n] = ch;
      }
      this.c.GPListen.addEventListener('characteristicvaluechanged', e => {
        const buf = e.target.value;
        for (const l of [...this.listeners]) if (l.match(buf)) l.fn(buf);
      });
      await this.c.GPListen.startNotifications();
      this.c.NordicListen.addEventListener('characteristicvaluechanged', e => this.nordic && this.nordic(e.target.value));
      await this.c.NordicListen.startNotifications();
      this.connected = true;
      this.keepAlive = setInterval(() => {
        if (!this.transferring && performance.now() - this.lastSend > 3000) this.gp([0x20, 0x06], [0x22]).catch(() => {});
      }, 1000);
      this.onchange && this.onchange();
    }
    disconnect() { if (this.device && this.device.gatt.connected) this.device.gatt.disconnect(); }

    listen(prefix, fn) {
      const l = { fn, match: buf => prefix.every((p, i) => buf.getUint8(i) === p) };
      this.listeners.add(l); return () => this.listeners.delete(l);
    }

    // Send a command; if a reply is expected, wait for it and resend if it doesn't come.
    gp(data, reply, timeoutMs = 2000, maxTries = 5) {
      return new Promise((resolve, reject) => {
        let tries = 0, done = false, timer = null, off = null;
        const finish = (fn, v) => { if (done) return; done = true; clearTimeout(timer); off && off(); fn(v); };
        if (reply) off = this.listen(reply, buf => finish(resolve, buf));
        const send = () => {
          if (done) return;
          tries++; this.lastSend = performance.now();
          this.c.GPWrite.writeValue(new Uint8Array(data)).then(() => {
            if (!reply) return finish(resolve);
            timer = setTimeout(() => tries >= maxTries ? finish(reject, new Error('No reply from Furby to ' + hex(data))) : send(), timeoutMs);
          }).catch(err => (!done && tries < maxTries) ? setTimeout(send, 200) : finish(reject, err));
        };
        send();
      });
    }

    action(a, b, c, d) { if (!this.connected) return Promise.resolve(); this.log('Action ' + [a, b, c, d].join('/')); return this.gp([0x13, 0, a, b, c, d]).catch(e => this.log(e.message)); }
    antenna(r, g, b) { if (!this.connected) return Promise.resolve(); return this.gp([0x14, r, g, b]).catch(() => {}); }
    eyes(on) { return this.gp([0xcd, on ? 1 : 0]); }

    async slots() {
      const buf = await this.gp([0x72], [0x72]);
      const filled = (buf.getUint8(3) << 8) | buf.getUint8(4), active = (buf.getUint8(7) << 8) | buf.getUint8(8);
      return Array.from({ length: 14 }, (_, i) => active & (1 << i) ? SLOT_ACTIVE : filled & (1 << i) ? SLOT_FILLED : SLOT_EMPTY);
    }
    async slotInfo(slot) {
      const buf = await this.gp([0x73, slot], [0x73, slot]);
      const info = { len: buf.getUint32(9) & 0xffffff };
      if (info.len > 0) {
        info.name = new TextDecoder().decode(new DataView(buf.buffer, 2, 8));
        info.checksum = buf.getUint32(13) >>> 0;
      }
      return info;
    }

    // Make sure this DLC is the active one on the Furby. Uploads it only if needed.
    async ensureDLC(url, progress) {
      const data = await (await fetch(url, { cache: 'no-store' })).arrayBuffer();
      const sum = adler32(data);
      const file = url.slice(url.lastIndexOf('/') + 1);
      const name12 = file.padStart(12, '_').toUpperCase();
      const short = name12.replace(/\.DLC$/, '').slice(-8);

      let slots = await this.slots();
      const active = slots.indexOf(SLOT_ACTIVE);
      if (active !== -1) {
        const info = await this.slotInfo(active);
        if (info.checksum === sum) { this.log(file + ' is already on your Furby.'); return; }
      }
      // Already uploaded but not active? Just activate it.
      for (let i = 0; i < 14; i++) {
        if (slots[i] !== SLOT_FILLED) continue;
        const info = await this.slotInfo(i);
        if (info.checksum === sum) {
          if (active !== -1) await this.gp([0x62, active], [0xdc]).catch(() => {});
          await this.activate(i); this.log(file + ' switched on.'); return;
        }
      }

      this.log('Clearing old DLCs off the Furby…');
      for (let i = 0; i < 14; i++) {
        if (slots[i] === SLOT_EMPTY) continue;
        await this.gp([0x62, i], [0xdc]).catch(() => {});
        await this.gp([0x74, i], [0x74]).catch(() => {});
      }
      await this.eyes(false).catch(() => {});
      this.log('Uploading ' + file + ' (' + Math.round(data.byteLength / 1024) + ' KB). Keep the Furby close…');
      try { await this.upload(data, name12, progress); }
      finally { await this.eyes(true).catch(() => {}); }

      await sleep(1500);
      slots = await this.slots();
      const slot = slots.indexOf(SLOT_FILLED);
      if (slot === -1) throw new Error('Upload failed: the Furby did not save the file.');
      const info = await this.slotInfo(slot);
      if (info.checksum !== sum) throw new Error('Upload got corrupted. Try again with the Furby closer.');
      await this.activate(slot);
      this.log(file + ' uploaded and switched on.');
    }
    async activate(slot) {
      await this.gp([0x60, slot], [0xdc], 45000, 1);
      await this.gp([0x61], [0xdc], 20000, 2);
    }

    upload(buf, name12, progress) {
      const size = buf.byteLength;
      const init = [0x50, 0, size >> 16 & 255, size >> 8 & 255, size & 255, 2, ...new TextEncoder().encode(name12), 0, 0];
      let pos = 0, rx = 0, fails = 0;
      return new Promise((resolve, reject) => {
        let off = null;
        const end = (fn, v) => { this.transferring = false; off && off(); this.nordic = null; this.c.NordicWrite.writeValue(new Uint8Array([9, 0, 0])).catch(() => {}); fn(v); };
        const next = () => {
          if (!this.transferring) return;
          if (rx > 10) return setTimeout(next, 100);
          const chunk = buf.slice(pos, pos + 20);
          const fw = this.c.FileWrite;
          const w = fw.properties.writeWithoutResponse && fw.writeValueWithoutResponse ? fw.writeValueWithoutResponse(chunk) : fw.writeValue(chunk);
          w.then(() => {
            this.lastSend = performance.now(); pos += chunk.byteLength;
            progress && progress(pos, size);
            if (pos < size) next();
            else setTimeout(() => { if (this.transferring) end(resolve); }, 10000);
          }).catch(err => ++fails <= 3 ? setTimeout(next, 16) : end(reject, err));
        };
        off = this.listen([0x24], b => {
          const mode = b.getUint8(1);
          if (mode === 3 || mode === 6) end(reject, new Error('The Furby stopped the upload.'));
          else if (mode === 5) end(resolve);
          else if (mode === 2) { this.transferring = true; next(); }
        });
        this.nordic = b => { if (b.getUint8(0) === 0x09) rx = b.getUint8(1); };
        this.c.NordicWrite.writeValue(new Uint8Array([9, 1, 0]))
          .then(() => this.c.GPWrite.writeValue(new Uint8Array(init)))
          .catch(err => end(reject, err));
      });
    }
  }

  function adler32(buf) {
    const d = new Uint8Array(buf); let a = 1, b = 0;
    for (let i = 0; i < d.length; i++) { a = (a + d[i]) % 65521; b = (b + a) % 65521; }
    return ((b << 16) | a) >>> 0;
  }

  window.FurbyBLE = FurbyBLE;
})();
