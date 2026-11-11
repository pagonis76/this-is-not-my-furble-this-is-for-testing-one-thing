// The Furby Simulator's brain: the real Furby Connect firmware running in furbulator
// (https://github.com/FurbLord/furbulator, MIT), compiled to WebAssembly (furby_core.js).
// This file feeds it touches, tilts and sounds, and shows what it does: the eyes on the two
// screens, its voice, the antenna light, and the motor's cam position moving ears, body and beak.
(() => {
  const DB = 'furbulator-web', STORE = 'files', KEY = 'rom';   // shared with ../emulator/
  const ROM_PATH = '/furby-connect.fcrom';

  function idb() {
    return new Promise((res, rej) => {
      const r = indexedDB.open(DB, 1);
      r.onupgradeneeded = () => r.result.createObjectStore(STORE);
      r.onsuccess = () => res(r.result);
      r.onerror = () => rej(r.error);
    });
  }
  async function romGet() {
    try { const db = await idb(); return await new Promise((res, rej) => { const q = db.transaction(STORE).objectStore(STORE).get(KEY); q.onsuccess = () => res(q.result); q.onerror = () => rej(q.error); }); }
    catch (e) { return undefined; }
  }
  async function romPut(name, data) {
    try { const db = await idb(); await new Promise((res, rej) => { const t = db.transaction(STORE, 'readwrite'); t.objectStore(STORE).put({ name, data }, KEY); t.oncomplete = res; t.onerror = () => rej(t.error); }); } catch (e) {}
  }

  // ---- sound: the DAC's 16 kHz stereo stream, resampled to the sound card ----
  class Speaker {
    constructor() {
      this.ctx = null; this.buf = new Float32Array(16000 * 2 * 2); this.r = 0; this.w = 0; this.rate = 16000; this.pos = 0;
      this.level = 0; this.playing = false;
    }
    start() {
      if (this.ctx) return this.ctx.resume();
      const AC = window.AudioContext || window.webkitAudioContext;
      this.ctx = new AC();
      const node = this.ctx.createScriptProcessor(2048, 0, 2);
      node.onaudioprocess = e => this.fill(e.outputBuffer);
      node.connect(this.ctx.destination);
      this.node = node;
      return this.ctx.resume();
    }
    queued() { return ((this.w - this.r + this.buf.length) % this.buf.length) / 2; }
    push(i16, frames, rate) {
      this.rate = rate;
      const cap = this.buf.length;
      for (let i = 0; i < frames * 2; i++) {
        this.buf[this.w] = i16[i] / 32768;
        this.w = (this.w + 1) % cap;
        if (this.w === this.r) this.r = (this.r + 2) % cap;   // overrun: drop the oldest
      }
      // Keep the delay bounded: past 0.5 s queued, skip ahead to 0.2 s.
      if (this.queued() > rate * 0.5) this.r = (this.w - Math.round(rate * 0.2) * 2 + cap * 4) % cap;
    }
    fill(out) {
      const L = out.getChannelData(0), R = out.getChannelData(1), cap = this.buf.length;
      const step = this.rate / this.ctx.sampleRate;
      let sum = 0;
      // Start (and restart after running dry) only with 0.15 s in hand, so a short slowdown
      // doesn't chop the sound into crackles.
      if (!this.playing && this.queued() >= this.rate * 0.15) this.playing = true;
      for (let i = 0; i < L.length; i++) {
        if (!this.playing || this.queued() < 2) { this.playing = false; L[i] = R[i] = 0; continue; }
        const f = this.pos, j = this.r, k = (this.r + 2) % cap;
        L[i] = this.buf[j] + (this.buf[k] - this.buf[j]) * f;
        R[i] = this.buf[j + 1] + (this.buf[k + 1] - this.buf[j + 1]) * f;
        sum += L[i] * L[i];
        this.pos += step;
        while (this.pos >= 1) { this.pos -= 1; this.r = (this.r + 2) % cap; }
      }
      this.level = Math.sqrt(sum / L.length);
    }
  }

  class EmuFurby {
    constructor(ui) {
      this.ui = ui;   // { leftCanvas, rightCanvas, onStatus(text) }
      this.mod = null; this.ready = false; this.speaker = new Speaker();
      this.cpi = 4;   // fixed: a steady speed keeps the sound clean (the firmware still keeps up) this.windowEmu = 0; this.windowWall = 0; this.windowBusy = 0; this.slow = 0; this.idle = 0;
      this.motor = { pos: 0, target: 0 };
      this.commits = [-1, -1];
      this.holds = new Map();
      this.dlcState = '';
    }
    async savedRom() { const s = await romGet(); return s && s.data ? s : null; }
    async load(romBytes, romName) {
      if (romName) romPut(romName, romBytes);
      this.ui.onStatus('Starting the Furby’s chip…');
      this.mod = await FurbyCore({ locateFile: f => f + '?v=2' });
      this.mod.FS.writeFile(ROM_PATH, romBytes);
      const M = this.mod;
      this.api = {
        init: M.cwrap('fc_init', 'number', ['string']),
        error: M.cwrap('fc_error', 'string', []),
        run: M.cwrap('fc_run', 'number', ['number', 'number']),
        setCpi: M.cwrap('fc_set_cpi', null, ['number']),
        off: M.cwrap('fc_powered_off', 'number', []),
        backlight: M.cwrap('fc_backlight', 'number', []),
        reboot: M.cwrap('fc_reboot', 'number', ['number']),
        eye: M.cwrap('fc_eye', 'number', ['number']),
        commits: M.cwrap('fc_eye_commits', 'number', ['number']),
        rate: M.cwrap('fc_audio_rate', 'number', []),
        take: M.cwrap('fc_audio_take', 'number', ['number', 'number']),
        motor: M.cwrap('fc_motor', 'number', []),
        led: M.cwrap('fc_led', 'number', []),
        sensor: M.cwrap('fc_sensor', null, ['number', 'number']),
        orient: M.cwrap('fc_orient', null, ['number', 'number', 'number']),
        shake: M.cwrap('fc_shake', null, ['number']),
        noise: M.cwrap('fc_noise', null, ['number']),
        mask: M.cwrap('fc_mask', null, ['number']),
        maskNow: M.cwrap('fc_mask_now', 'number', []),
        ble: M.cwrap('fc_ble', null, ['number', 'number']),
        dlc: M.cwrap('fc_dlc', 'number', ['string', 'string']),
        dlcState: M.cwrap('fc_dlc_state', 'number', []),
        dlcProgress: M.cwrap('fc_dlc_progress', 'number', []),
        listen: M.cwrap('fc_listen_take', 'number', ['number', 'number']),
      };
      this.audioPtr = M._malloc(2 * 2 * 8192);
      this.bytesPtr = M._malloc(64);
      if (!this.api.init(ROM_PATH)) throw new Error(this.api.error() || 'the ROM pack did not load');
      this.api.setCpi(this.cpi);
      this.ready = true;
      this.last = performance.now();
      this.ui.onStatus('');
      return true;
    }

    // One animation frame: run the chip for the real time that passed (never faster than real
    // time), then collect its sound, eyes, light and motor.
    frame(now) {
      if (!this.ready) return;
      const dt = Math.min(0.1, (now - this.last) / 1000);
      this.last = now;
      const a = this.api;
      const t0 = performance.now();
      // While a DLC is uploading, run as fast as the PC allows (sound muted) so it takes less time.
      const turbo = this.uploading;
      const done = a.off() ? dt : a.run(turbo ? 0.1 : dt, turbo ? 12 : 14);
      const busy = performance.now() - t0;
      this.autoBoost(dt, done, busy);
      for (const [i, until] of this.holds) if (now >= until) { a.sensor(i, 0); this.holds.delete(i); }
      // sound
      const M = this.mod;
      for (;;) {
        const n = a.take(this.audioPtr, 8192);
        if (n <= 0) break;
        if (!turbo) this.speaker.push(new Int16Array(M.HEAPU8.buffer, this.audioPtr, n * 2), n, a.rate());
        if (n < 8192) break;
      }
      // eyes
      // The backlight line's polarity is only inferred, so the eyes stay lit unless he is off.
      const dim = a.off() ? 0 : 1;
      for (let p = 0; p < 2; p++) {
        const c = a.commits(p);
        if (c === this.commits[p] && dim === this.dimWas) continue;
        this.commits[p] = c;
        this.drawEye(p, dim);
      }
      this.dimWas = dim;
      // motor: the firmware sends cam positions; the cam turns towards them
      this.motor.target = a.motor();
      const speed = 900 * dt;   // cam units per frame (about half a turn per 0.25 s)
      const d = this.motor.target - this.motor.pos;
      this.motor.pos += Math.abs(d) <= speed ? d : Math.sign(d) * speed;
      this.ledRaw = a.led();
      this.pumpDlc();
    }
    autoBoost(dt, done, busy) {
      this.windowEmu += done; this.windowWall += dt; this.windowBusy += busy / 1000;
      if (this.windowWall < 0.5) return;
      const speed = this.windowEmu / this.windowWall, load = this.windowBusy / this.windowWall;
      this.windowEmu = this.windowWall = this.windowBusy = 0;
      if (this.windows = (this.windows || 0) + 1, this.windows > 1) this.speed = speed;
    }
    drawEye(panel, dim) {
      const cv = panel === 0 ? this.ui.leftCanvas : this.ui.rightCanvas;
      const g = cv.getContext('2d');
      if (!this.img) this.img = [g.createImageData(128, 128), g.createImageData(128, 128)];
      const img = this.img[panel], px = img.data;
      const src = new Uint32Array(this.mod.HEAPU8.buffer, this.api.eye(panel), 128 * 128);
      for (let i = 0; i < src.length; i++) {
        const c = src[i], o = i * 4;
        px[o] = ((c >> 16) & 255) * dim; px[o + 1] = ((c >> 8) & 255) * dim; px[o + 2] = (c & 255) * dim; px[o + 3] = 255;
      }
      g.putImageData(img, 0, 0);
    }
    get poweredOff() { return this.ready && !!this.api.off(); }
    wake() { if (this.poweredOff) { this.api.reboot(1); this.commits = [-1, -1]; } }

    // ---- inputs ----
    // Sensors: 0-3 antenna (pushed in four directions), 4 tongue, 5 tail.
    press(i, ms = 400) {
      if (!this.ready) return;
      if (this.poweredOff) { this.wake(); return; }
      this.api.sensor(i, 1);
      this.holds.set(i, performance.now() + ms);
    }
    shake(ms = 1200) { if (!this.ready) return; this.wake(); this.api.shake(1); setTimeout(() => this.api.shake(0), ms); }
    orient(o, tx = 0, tz = 0) { if (!this.ready) return; this.wake(); this.api.orient(o, tx, tz); }
    noise(ms = 1000) { if (!this.ready) return; this.wake(); this.api.noise(1); setTimeout(() => this.api.noise(0), ms); }
    mask(entry) { if (this.ready) this.api.mask(entry); }
    ble(bytes) {
      if (!this.ready) return;
      this.mod.HEAPU8.set(bytes, this.bytesPtr);
      this.api.ble(this.bytesPtr, bytes.length);
    }
    listenAll() {
      const out = [];
      for (;;) {
        const n = this.api.listen(this.bytesPtr, 64);
        if (n < 0) break;
        out.push(Array.from(this.mod.HEAPU8.subarray(this.bytesPtr, this.bytesPtr + n)));
      }
      return out;
    }
    // ---- DLCs, sent the way the Furby Connect app does: upload to slot 2, load, activate ----
    async uploadDlc(bytes, name, onStep) {
      if (!this.ready) throw new Error('Wake Furby up first');
      this.wake();
      const safe = name.replace(/[^A-Za-z0-9._-]/g, '_').slice(-12).toUpperCase();
      // Same steps as the real app (furble.js): clear the old DLCs, upload,
      // then load and switch on whichever slot the Furby stored it in.
      onStep && onStep('Clearing old DLCs off Furby…');
      const before = await this.slots();
      for (let i = 0; i < 16; i++) {
        if (!(before.filled & (1 << i)) && !(before.active & (1 << i))) continue;
        await this.command([0x62, i], 0xDC, 5000).catch(() => {});
        await this.command([0x74, i], 0x74, 5000).catch(() => {});
      }
      this.mod.FS.writeFile('/upload.dlc', bytes);
      this.listenAll();
      if (!this.api.dlc('/upload.dlc', safe)) throw new Error('the Furby would not take the file');
      this.uploading = true;
      try {
        await this.waitFor(() => {
          const s = this.api.dlcState();
          if (s === 5) throw new Error('the upload failed');
          onStep && onStep('Sending ' + name + ' to Furby… ' + Math.floor(this.api.dlcProgress() * 100) + '%');
          return s === 4;
        }, 900000);
      } finally { this.uploading = false; }
      onStep && onStep('Checking it…');
      await this.waitFor(() => false, 1500).catch(() => {});   // the Furby may still reject it
      const after = await this.slots();
      const slot = [...Array(16).keys()].find(i => after.filled & (1 << i));
      if (slot === undefined) throw new Error(bytes.length > 2500000
        ? 'Furby threw it away, it is too big (' + (bytes.length / 1048576).toFixed(1) + ' MB). DLCs up to about 2 MB work.'
        : 'Furby threw the file away');
      onStep && onStep('Loading it…');
      await this.command([0x60, slot], 0xDC, 45000);
      onStep && onStep('Switching it on…');
      await this.command([0x61], 0xDC, 20000);
      const now = await this.slots();
      if (!(now.active & (1 << slot))) throw new Error('Furby would not switch it on');
    }
    async slots() {
      const p = await this.command([0x72], 0x72, 5000);
      return { filled: (p[3] << 8) | p[4], active: (p[7] << 8) | p[8] };
    }
    async command(bytes, reply, ms) {
      this.listenAll();
      this.ble(bytes);
      let got = null;
      await this.waitFor(() => { got = got || this.listenAll().find(p => p[0] === reply); return !!got; }, ms);
      return got;
    }
    waitFor(test, ms) {
      const t0 = performance.now();
      return new Promise((res, rej) => {
        const tick = () => {
          try { if (test()) return res(); } catch (e) { return rej(e); }
          if (performance.now() - t0 > ms) return rej(new Error('Furby did not answer'));
          setTimeout(tick, 100);
        };
        tick();
      });
    }
    pumpDlc() {}
    action(a) { this.wake(); this.ble([0x13, 0x00, a[0], a[1], a[2], a[3]]); }
  }
  window.EmuFurby = EmuFurby;
})();
