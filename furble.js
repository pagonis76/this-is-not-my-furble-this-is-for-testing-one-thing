const fluff_service = 'dab91435-b5a1-e29c-b041-bcd562613bde';

var furby_uuids = {
    'GeneralPlusListen': 'dab91382-b5a1-e29c-b041-bcd562613bde',
    'GeneralPlusWrite':  'dab91383-b5a1-e29c-b041-bcd562613bde',
    'NordicListen':      'dab90756-b5a1-e29c-b041-bcd562613bde',
    'NordicWrite':       'dab90757-b5a1-e29c-b041-bcd562613bde',
    'RSSIListen':        'dab90755-b5a1-e29c-b041-bcd562613bde',
    'F2FListen':         'dab91440-b5a1-e29c-b041-bcd562613bde',
    'F2FWrite':          'dab91441-b5a1-e29c-b041-bcd562613bde',
    'FileWrite':         'dab90758-b5a1-e29c-b041-bcd562613bde'
}

var file_transfer_modes = {
    1: 'EndCurrentTransfer',
    2: 'ReadyToReceive',
    3: 'FileTransferTimeout',
    4: 'ReadyToAppend',
    5: 'FileReceivedOk',
    6: 'FileReceivedErr'
}

const SLOT_EMPTY = 0;
const SLOT_UPLOADING = 1;
const SLOT_FILLED = 2;
const SLOT_ACTIVE = 3;

function flipDict(d) {
    let flipped = {};
    for (let k in d) {
      let v = d[k];
      flipped[v] = k;
    }
    return flipped;
}

var uuid_lookup = flipDict(furby_uuids);
var file_transfer_lookup = flipDict(file_transfer_modes);
var device;
var isConnected = false;
var isTransferring = false;
var furby_chars = {};
var furbies = [];
var sendTarget = 'all';
var furbyCount = 0;
var userDisconnect = false;
function targets() {
    var ready = furbies.filter(f => f.ready);
    if (sendTarget === 'all') return ready;
    return ready.filter(f => f.id == sendTarget);
}
// Each Furby gets its own queue so two writes never hit the same Furby at
// once (Chrome rejects that with "GATT operation already in progress").
// A busy write is retried a few times before giving up.
function writeTo(f, data) {
    var bytes = new Uint8Array(data);
    var run = async () => {
        for (var i = 0; ; i++) {
            try {
                await f.chars.GeneralPlusWrite.writeValue(bytes);
                f.lastSent = performance.now();
                return;
            } catch (e) {
                if (i >= 3 || !f.ready) throw e;
                await sleep(80 * (i + 1));
            }
        }
    };
    var p = (f.queue || Promise.resolve()).then(run, run);
    f.queue = p.catch(() => {});
    return p;
}
function writeGP(data) {
    var t = targets();
    if (t.length == 0) return Promise.reject(new Error('No Furby selected'));
    return Promise.all(t.map(f => writeTo(f, data)));
}
function updateTargets() {
    var sel = document.getElementById('sendto');
    if (!sel) return;
    var html = "<option value='all'>All Furbys (" + furbies.length + ")</option>";
    furbies.forEach(f => { html += "<option value='" + f.id + "'>" + f.name + (f.ready ? '' : ' (reconnecting)') + "</option>"; });
    sel.innerHTML = html;
    if (sendTarget !== 'all' && !furbies.some(f => f.id == sendTarget)) sendTarget = 'all';
    sel.value = sendTarget;
    var st = document.getElementById('state');
    if (furbies.length > 1) st.textContent = furbies.length + ' Furbys connected';
}
async function onOneDisconnected(f) {
    if (!f.ready) return;
    f.ready = false;
    if (!userDisconnect) {
        log('> ' + f.name + ' dropped, reconnecting...');
        msg.error(f.name + ' dropped, reconnecting...');
        updateTargets();
        for (var i = 0; i < 6 && !userDisconnect; i++) {
            await sleep(500 + i * 700);
            try {
                await setupFurby(f);
                f.ready = true;
                log('> ' + f.name + ' reconnected');
                msg.ok(f.name + ' reconnected!');
                updateTargets();
                return;
            } catch (e) {
                log(f.name + ' reconnect try ' + (i + 1) + ' failed: ' + e.message);
            }
        }
    }
    furbies = furbies.filter(x => x !== f);
    log('> ' + f.name + ' disconnected');
    if (furbies.length == 0) { onDisconnected(); updateTargets(); return; }
    furby_chars = furbies[0].chars;
    device = furbies[0].dev;
    msg.error(f.name + ' disconnected');
    updateTargets();
}
var gp_listen_callbacks = [];
var nordicListener = null;
var keepAliveTimer = null;
var lastCommandSent = 0;
var NO_RESPONSE = Symbol();


function log() {
    let bits = []
    for (let arg of arguments) {
        if (DataView.prototype.isPrototypeOf(arg))
            bits.push(buf2hex(arg))
        else
            bits.push(''+arg)
    }
    var s = bits.join(' ')
    console.log(s);
    let o = document.getElementById('out');
    o.textContent += s + "\n";
    o.scrollTop = o.scrollHeight;
}

// Sends a command to the Furby. If a reply is expected and none arrives
// within GP_TIMEOUT_MS, the command is sent again (the Furby sometimes
// ignores commands, e.g. right after a DLC upload while it saves the file).
const GP_TIMEOUT_MS = 2000;
const GP_MAX_TRIES = 5;
function sendGPCmd(data, response_prefix, timeoutMs = GP_TIMEOUT_MS, maxTries = GP_MAX_TRIES) {
    return new Promise((resolve, reject) => {
        if (data.length != 2 && data[0] != 0x20 && data[1] != 0x06)
            log('Sending data to GeneralPlusWrite', buf2hex(data));
        var hnd;
        var timer = null;
        var tries = 0;
        var done = false;
        let finish = (fn, val) => {
            if (done) return;
            done = true;
            if (timer) clearTimeout(timer);
            if (hnd !== undefined) removeGPListenCallback(hnd);
            fn(val);
        };
        if (response_prefix != NO_RESPONSE) {
            hnd = addGPListenCallback(response_prefix, buf => finish(resolve, buf));
        }
        let send = () => {
            if (done) return;
            tries++;
            lastCommandSent = performance.now();
            writeGP(data)
                .then(() => {
                    // if we're not expecting a response resolve once the data's been sent
                    if (response_prefix == NO_RESPONSE) {
                        finish(resolve);
                        return;
                    }
                    if (done) return;
                    timer = setTimeout(() => {
                        if (done) return;
                        if (tries >= maxTries) {
                            finish(reject, new Error('No reply from Furby to command ' + buf2hex(data)));
                        } else {
                            log('No reply yet, resending', buf2hex(data));
                            send();
                        }
                    }, timeoutMs);
                }).catch(error => {
                    // Bluetooth busy: wait a moment and try again
                    if (!done && tries < maxTries) {
                        setTimeout(send, 200);
                    } else {
                        finish(reject, error);
                    }
                });
        };
        send();
    });
}

function addGPListenCallback(response_prefix, fn) {
    let handle = gp_listen_callbacks.length;
    gp_listen_callbacks[handle] = [fn, response_prefix];
    return handle;
}

function removeGPListenCallback(handle) {
    delete gp_listen_callbacks[handle];
}

function handleGeneralPlusResponse(event) {
    let buf = event.target.value;
    if (buf.getUint8(0) != 0x22) // don't spam log with ImHereSignal keepalive responses TODO: decode this packet
        log('Got GeneralPlus response', buf);
    else {
        console.log(buf2hex(buf));
    }
    for (let handle in gp_listen_callbacks) {
        let [cb, prefix] = gp_listen_callbacks[handle];
        if (prefixMatches(prefix, buf))
            cb(buf);
    }
}

function prefixMatches(prefix, buf) {
    if (typeof(prefix) == 'undefined')
        return true;

    for (let i=0; i < prefix.length; i++) {
        if (buf.getUint8(i) != prefix[i])
            return false;
    }
    return true;
}

function triggerAction(input, index, subindex, specific) {
    if (arguments.length == 1)
        data = [0x10, 0, input];
    else if (arguments.length == 2)
        data = [0x11, 0, input, index];
    else if (arguments.length == 3)
        data = [0x12, 0, input, index, subindex];
    else if (arguments.length == 4)
        data = [0x13, 0, input, index, subindex, specific];
    else 
        throw 'Must specify at least an input';
    return sendGPCmd(data, NO_RESPONSE);
}

function customAction() {
    let args = [];
    document.querySelectorAll('input[name=furby-action]').forEach(el => args.push(parseInt(el.value)));
    console.log(args);
    triggerAction.apply(null, args);
}
function enableEyes(b) {
    return sendGPCmd([0xcd, b ? 1 : 0], NO_RESPONSE);
}
// Loading a big DLC can take the Furby a long time, and resending the
// command while it is still loading only starts it over. So wait up to
// 45 s for one reply instead of resending every 2 s.
function loadDLC(slot) {
    return sendGPCmd([0x60, slot], [0xdc], 45000, 1);
}

function activateDLC() {
    return sendGPCmd([0x61], [0xdc], 20000, 2);
}

function deactivateDLC(slot) {
    return sendGPCmd([0x62, slot], [0xdc]);
}

async function loadAndActivateDLC(slot) {
    await loadDLC(slot);
    await activateDLC();
}

async function getDLCInfo() {
    let buf = await sendGPCmd([0x72], [0x72]);
    log('dlc info: ', buf);
    let filledSlots = (buf.getUint8(3) << 8) |  buf.getUint8(4);
    let activeSlots = (buf.getUint8(7) << 8) |  buf.getUint8(8);
    let slots = [];
    for (let i=0; i < 14; i++) {
        slots[i] = 0;
        if (filledSlots & (1 << i))
            slots[i] = SLOT_FILLED;
        if (activeSlots & (1 << i))
            slots[i] = SLOT_ACTIVE;
    }
    return slots;
}

async function getAllDLCInfo() {
    let allSlotsInfo = await getDLCInfo();
    var slots = [];
    for (let i=0; i < allSlotsInfo.length; i++) {
        if (allSlotsInfo[i] != SLOT_EMPTY) {
            slots[i] = await getDLCSlotInfo(i);
            slots[i].status = allSlotsInfo[i];
        }
    }
    console.log(slots);
}

async function getActiveSlotInfo() {
    let allSlotsInfo = await getDLCInfo();
    let activeSlot = allSlotsInfo.indexOf(SLOT_ACTIVE);
    if (activeSlot == -1) return null;
    return await getDLCSlotInfo(activeSlot);
}


async function getDLCSlotInfo(slot) {
    let buf = await sendGPCmd([0x73, slot], [0x73, slot]);
    //log('Got slot info', buf);
    var props = {};
    props.len = buf.getUint32(9) & 0xffffff;
    if (props.len > 0) {
        let namebuf = new DataView(buf.buffer, 2, 8);
        let decoder = new TextDecoder('utf-8');
        props.name = decoder.decode(namebuf);
        props.checksum =  buf.getUint32(13);
        log(`slot ${slot} name: ${props.name}, length: ${props.len} chksum: 0x` + props.checksum.toString(16));
    } else {
        log(`slot ${slot} is empty`);
    }
    
    return props;
    // 73 0d 5455303032393439 04c95a 2754ea15 - 73, slot number, filename (minus extension), length, adler32 checksum
}

function deleteDLC(slot) {
    return sendGPCmd([0x74, slot], [0x74]);
}

async function deleteAllDLCSlots() {
    // Only touch slots that actually hold a DLC, and never let a slow
    // reply from the Furby stop the upload from going ahead.
    let slots = null;
    try {
        slots = await getDLCInfo();
    } catch (e) {
        log('Could not read DLC slots, clearing all of them: ' + (e && e.message ? e.message : e));
    }
    for (let i=0; i<14; i++) {
        if (slots && slots[i] == SLOT_EMPTY) continue;
        try { await deactivateDLC(i); } catch (e) { log(`slot ${i}: deactivate got no reply, carrying on`); }
        try { await deleteDLC(i); } catch (e) { log(`slot ${i}: delete got no reply, carrying on`); }
    }
}

async function getFirmwareVersion() {
    let buf = await sendGPCmd([0xfe], [0xfe]);
    let version = buf.getUint8(1)
    log('Firmware version ', version);
    return version;
}

// Personality switch for Furbys running the patched firmware (furby-personality patch).
// Command 0x15 takes the two mask readings a real mask would give; 00 00 clears it.
const PERSONALITY_READINGS = [[0x00,0x00],[0xe3,0x91],[0xc7,0x94],[0xb1,0x97],[0x9b,0x9b],[0x80,0x9f],[0x62,0xa0]];
const PERSONALITY_NAMES = ['Normal','DJ','Princess','Ninja','Pirate','Cat','PopStar'];
function setPersonality(i) {
    let r = PERSONALITY_READINGS[i];
    log('Setting personality to ' + PERSONALITY_NAMES[i]);
    return sendGPCmd([0x15, r[0], r[1]], NO_RESPONSE);
}

function setAntennaColor(r, g, b) {
    log(`Setting antenna color to (${r}, ${g}, ${b})`);
    return sendGPCmd([0x14, r, g, b], NO_RESPONSE);
}

function cycleDebug() {
    return sendGPCmd([0xdb], NO_RESPONSE);
}

function startKeepAlive() {
    clearInterval(keepAliveTimer);
    keepAliveTimer = setInterval(() => {
        var now = performance.now();
        furbies.forEach(f => {
            // only the Furby receiving a file skips its keep-alive
            if (isTransferring && f.chars === furby_chars) return;
            if (f.ready && now - (f.lastSent || 0) > 2500)
                writeTo(f, [0x20, 0x06]).catch(() => {});
        });
    }, 1000);
}

function setNordicNotifications(enable, cb) {
    let data = [9, (enable ? 1 : 0), 0];
    nordicListener = enable ? cb : null;
    return furby_chars.NordicWrite.writeValue(new Uint8Array(data));
}

function handleNordicNotification(event) {
    //log('Nordic listen', buf);
    if (nordicListener) 
        nordicListener(event.target.value);
}

function makeDLCFilename(dlcurl) {
    return dlcurl.substr(dlcurl.lastIndexOf('/') + 1).padStart(12, '_').toUpperCase();
}

async function fetchDLC(dlcurl) {
    let response = await fetch(dlcurl, {cache: 'no-store'});
    if (response.status != 200) {
        msg.error('DLC not found on server');
        throw new Error('Failed to fetch DLC ' + dlcurl);
    }
    let buf = await response.arrayBuffer();
    log('Fetched DLC from server:', dlcurl, ' checksum 0x' + adler32(buf).toString(16));
    return buf;
}

// Uploads a DLC to the current Furby without switching it on.
// Throws if the upload fails or the saved file doesn't match.
async function uploadDLCOnly(buf, dlcurl, label) {
    label = label || '';
    let chksumOriginal = adler32(buf);
    var progress = document.getElementById('dlcprogress');
    progress.max = buf.byteLength;
    try {
        progress.style.display = 'block';
        progress.removeAttribute('value');
        let c = 0;
        log('Clearing all DLC slots...');
        msg.ok(label + 'Clearing old DLCs off the Furby...');
        await deleteAllDLCSlots();
        await enableEyes(false); // eyes off, save battery
        await setAntennaColor(0,0,0);
        let name = makeDLCFilename(dlcurl);
        let started = false;
        await uploadDLC(buf, name, (current, total, maxRx) => {
            if (c % 100 == 0) 
                progress.value = current;
            if (c % 500 == 0)
                console.log(`transfer: ${current}/${total} maxRx:${maxRx}`);
            if (!started) {
                started = true;
                msg.ok(label + 'Eyes off while Uploading...');
            }
            c++; 
        });
    } finally {
        progress.style.display = 'none';
        try { await enableEyes(true); } catch (e) {} // eyes on
    }
    await sleep(1500); // let the Furby finish saving the file
    let slots = await getDLCInfo();
    let filledSlot = slots.indexOf(SLOT_FILLED);
    if (filledSlot == -1)
        throw new Error('Upload failed - no slots filled');
    log('DLC was uploaded to slot ' + filledSlot);
    let slotInfo = await getDLCSlotInfo(filledSlot);
    if (slotInfo.checksum != chksumOriginal)
        throw new Error('Expected checksum of 0x' + chksumOriginal.toString(16) + ' but got 0x' + slotInfo.checksum.toString(16));
    return filledSlot;
}

// Switches on a DLC that uploadDLCOnly put in the given slot.
async function activateUploadedDLC(filledSlot) {
    let slots = await getDLCInfo();
    let activeSlot = slots.indexOf(SLOT_ACTIVE);
    if (activeSlot != -1 && activeSlot != filledSlot) {
        log('deactivating old DLC in slot ' + activeSlot);
        await deactivateDLC(activeSlot);
    }
    await loadAndActivateDLC(filledSlot);
    slots = await getDLCInfo();
    if (slots.indexOf(SLOT_ACTIVE) != filledSlot)
        throw new Error('Failed to activate');
}

async function fetchAndUploadDLC(dlcurl) {
    try {
        let buf = await fetchDLC(dlcurl);
        let slot = await uploadDLCOnly(buf, dlcurl);
        msg.ok('DLC uploaded! Activating...');
        await activateUploadedDLC(slot);
        msg.ok('DLC activated!');
        await setAntennaColor(0,255,0);
    } catch (e) {
        msg.error('DLC upload failed :( ' + (e && e.message ? e.message : e));
        log(e && e.message ? e.message : e);
        await setAntennaColor(255,0,0).catch(() => {});
    }
}

function adler32(buf) {
    const MOD_ADLER = 65521;
    let dv = new DataView(buf);
    let a=1, b=0;
    for (let i=0; i < buf.byteLength; i++) {
        a = (a + dv.getUint8(i)) % MOD_ADLER;
        b = (b + a) % MOD_ADLER;
    }
    let checksum = (b << 16) >>> 0;
    return (checksum | a) >>> 0;
}

async function adler32test(url) {
    let resp = await fetch(url);
    let buf = await resp.arrayBuffer();
    console.log(buf);
    let checksum = adler32(buf);
    console.log('0x' + checksum.toString(16));
}

function uploadDLC(dlcbuf, filename, progresscb) {
    if (isTransferring) return Promise.reject('Transfer already in progress');
    let size = dlcbuf.byteLength;
    if (filename.length != 12)
        return Promise.reject('Filename must be 12 chars long');
    let initcmd = [0x50, 0x00,
        size >> 16 & 0xff, size >> 8 & 0xff, size & 0xff, 
        2];
    let encoder = new TextEncoder('utf-8');
    initcmd = initcmd.concat(Array.from(encoder.encode(filename)));
    initcmd = initcmd.concat([0,0]);
    isTransferring = false;
    let sendPos = 0;
    let rxPackets = 0;
    let CHUNK_SIZE = 20;
    let MAX_BUFFERED_PACKETS = 10;
    let maxRx = 0;
    let failedWrites = 0;

    return new Promise((resolve, reject) => {
        let transferNextChunk = () => {
            if (!isTransferring)
                return;
            if (rxPackets > MAX_BUFFERED_PACKETS) {
                log(`rxPackets=${rxPackets}, pausing...`);
                setTimeout(transferNextChunk, 100);
                return;
            }
            let chunk = dlcbuf.slice(sendPos, sendPos + CHUNK_SIZE);
            if (chunk.byteLength > 0) {
                let fw = furby_chars.FileWrite;
                let write = (fw.properties.writeWithoutResponse && fw.writeValueWithoutResponse)
                    ? fw.writeValueWithoutResponse(chunk) : fw.writeValue(chunk);
                write.then(() => {
                    lastCommandSent = performance.now();
                    sendPos += chunk.byteLength;
                    if (progresscb)
                        progresscb(sendPos, size, maxRx);
                    if (sendPos < size)
                        transferNextChunk(); // no 1 ms timer: browsers stretch it to 4 ms+, which slowed uploads a lot
                    else {
                        log('Sent final packet');
                        // If the Furby never confirms, carry on after a while anyway;
                        // the checksum check afterwards will catch a bad upload.
                        setTimeout(() => {
                            if (!isTransferring) return;
                            log('No upload confirmation from Furby, carrying on');
                            isTransferring = false;
                            removeGPListenCallback(hnd);
                            setNordicNotifications(false);
                            resolve();
                        }, 10000);
                    }
                }).catch(error => {
                    //removeGPListenCallback(hnd)
                    console.log(error);
                    if (++failedWrites <= 3) {
                        log('FileWrite.writeValue failed, will retry');
                        setTimeout(transferNextChunk, 16);
                    } else {
                        log('FileWrite.writeValue failed, giving up after too many failures');
                        isTransferring = false;
                        removeGPListenCallback(hnd);
                        reject(error);
                    }
                    
                    //reject(error);
                });  
            } else {
                log('tried to send empty packet??');
                isTransferring = false;
            }
        }

        let hnd = addGPListenCallback([0x24], buf => {
            let fileMode = buf.getUint8(1);
            log('Got FileWrite callback: ' + file_transfer_modes[fileMode]);
            if (fileMode == file_transfer_lookup.FileTransferTimeout ||
                fileMode == file_transfer_lookup.FileReceivedErr) {
                isTransferring = false;
                removeGPListenCallback(hnd);
                setNordicNotifications(false);
                reject('File Transfer error');
            } else if (fileMode == file_transfer_lookup.FileReceivedOk) {
                // TODO: sometimes we get a FileReceivedErr after getting FileReceivedOk
                // so we should add a delay before resolving to allow for failure
                log(`sendPos: ${sendPos} / ${size}`);
                isTransferring = false;
                removeGPListenCallback(hnd);
                setNordicNotifications(false);
                resolve();
            } else if (fileMode == file_transfer_lookup.ReadyToReceive) {
                isTransferring = true;
                transferNextChunk();
            }
        });

        let nordicCallback = (buf) => {
            let code = buf.getUint8(0);
            if (code == 0x09) {
                rxPackets = buf.getUint8(1);
                if (rxPackets > maxRx) maxRx = rxPackets;
                //console.log(`NordicListen GotPacketAck ${rxPackets}`);
                //transferNextChunk();
            } else if (code == 0x0a) {
                log('NordicListen GotPacketOverload', buf);
            } else {
                log('NordicListen uknown', buf);
            }
        };

        setNordicNotifications(true, nordicCallback).then(() => {
            log('Sending init DLC: ', buf2hex(initcmd), 'file length 0x' + size.toString(16));
            furby_chars.GeneralPlusWrite.writeValue(new Uint8Array(initcmd)).catch(error => reject(error));
        }).catch(error => reject(error));
    });
}

function onDisconnected() {
    log('> Bluetooth Device disconnected');
    msg.error('Device Disconnected');
    clearInterval(keepAliveTimer);
    isConnected = false;
    document.getElementById('connbtn').textContent = 'Connect';
    document.getElementById('state').textContent = 'Not Connected';
    enableButtons(false);
    
}

function enableButtons(enabled) {
    document.querySelectorAll('#furby-buttons button, #furby-buttons select').forEach(
        el => {
            if (enabled)
                el.removeAttribute('disabled')
            else
                el.setAttribute('disabled', 'disabled')
        });
}

function onConnected() {
    startKeepAlive();
    isConnected = true;
    msg.ok('Connected!');
    document.getElementById('connbtn').textContent = 'Disconnect';
    document.getElementById('state').textContent = 'Connected';
    enableButtons(true);
}

function buf2hex(dv) {
    var s = '';
    if (DataView.prototype.isPrototypeOf(dv)) {
        for (var i=0; i < dv.byteLength; i++) {
            s += ('0' + dv.getUint8(i).toString(16)).substr(-2);
        }
    } else if (Array.prototype.isPrototypeOf(dv)) {
        for (var i=0; i < dv.length; i++) {
            s += ('0' + dv[i].toString(16)).substr(-2);
        }    
    }
    return s;
}

function doConnectDisconnect() {
    isConnected ? doDisconnect() : doConnect();
}

async function doDisconnect() {
    try {
      log('Disconnecting from GATT Server...');
      userDisconnect = true;
      furbies.slice().forEach(f => f.dev.gatt.disconnect());
    } catch (e) {
      log('Argh! ' + e);
    }
}

function sleep(t) {
    return new Promise(resolve  => setTimeout(resolve, t));
}

async function checkActiveSlot() {
    let activeSlot = await getActiveSlotInfo();
    if (activeSlot) {
        let filename = activeSlot.name.replace(/^_+/, '') + '.dlc'.toUpperCase();
        log('current active slot is ' + activeSlot.name + ' looking for ' + filename);
        for (k in dlcdata) {
            if (k.toUpperCase() == filename) {
                log('setting up buttons for ' + k);
                setupDLCButtons(dlcdata[k]);
            }
        }
    }
}

function onFurbySensorData(buf) {
    let state = decodeFurbyState(buf);
    console.log(state);
    let ul = document.getElementById('sensor-state');
    //ul.outerHTML = '<ul></ul>';
    while (ul.firstChild) {
        ul.removeChild(ul.firstChild);
    }

    for (k in state) {
        let v = state[k];
        let li = document.createElement('li');
        li.textContent = `${k}: ${v}`;
        ul.appendChild(li);
    }
}

function decodeFurbyState(buf) {
    let antenna = '';
    if (buf.getUint8(1) & 2)
        antenna = 'left';
    if (buf.getUint8(1) & 1)
        antenna = 'right';
    if (buf.getUint8(2) == 0xc0) // fwd | back
        antenna = 'down';
    else if (buf.getUint8(2) & 0x40)
        antenna = 'forward';
    else if (buf.getUint8(2) & 0x80)
        antenna = 'back';

    let orientation = '';
    if (buf.getUint8(4) & 1) 
        orientation = 'upright';
    else if (buf.getUint8(4) & 2) 
        orientation = 'upside-down';   
    else if (buf.getUint8(4) & 4) 
        orientation = 'lying on right side';   
    else if (buf.getUint8(4) & 8) 
        orientation = 'lying on left side';      
    else if (buf.getUint8(4) & 0x20) 
        orientation = 'leaning back';
    else if (buf.getUint8(4) & 0x40) 
        orientation = 'tilted right'; 
    else if (buf.getUint8(4) & 0x80) 
        orientation = 'tilted left';    
    var state = {};
    state['antenna'] = antenna;
    state['orientation'] = orientation;

    if (buf.getUint8(2) & 1)
        state.tickle_head_back = true;
    if (buf.getUint8(2) & 2)
        state.tickle_tummy = true;
    if (buf.getUint8(2) & 4)
        state.tickle_right_side = true;
    if (buf.getUint8(2) & 8)
        state.tickle_left_side = true;
    if (buf.getUint8(2) & 0x10)
        state.pull_tail = true;
    if (buf.getUint8(2) & 0x20)
        state.push_tongue = true;
    return state;
}

// Connects GATT and sets up notifications for one Furby. Android often
// fails the first connect attempt when other Furbys are already connected,
// so it retries a few times.
async function setupFurby(entry) {
    var server, lastErr;
    for (var i = 0; i < 4; i++) {
        try {
            if (entry.dev.gatt.connected) entry.dev.gatt.disconnect();
            server = await entry.dev.gatt.connect();
            break;
        } catch (e) {
            lastErr = e;
            log(entry.name + ' connect try ' + (i + 1) + ' failed: ' + e.message);
            await sleep(400 + i * 600);
        }
    }
    if (!server) throw lastErr;
    log('Getting Furby Service...');
    const service = await server.getPrimaryService(fluff_service);
    const characteristics = await service.getCharacteristics();
    entry.chars = {};
    for (const characteristic of characteristics) {
        var name = uuid_lookup[characteristic.uuid];
        entry.chars[name] = characteristic;
    }
    entry.chars.GeneralPlusListen.addEventListener('characteristicvaluechanged', handleGeneralPlusResponse);
    await entry.chars.GeneralPlusListen.startNotifications();
    entry.chars.NordicListen.addEventListener('characteristicvaluechanged', handleNordicNotification);
    await entry.chars.NordicListen.startNotifications();
    entry.queue = Promise.resolve();
    entry.lastSent = performance.now();
    if (furbies[0] === entry || furbies.length == 0) furby_chars = entry.chars;
}

async function doConnect() {
    log('Requesting Bluetooth Devices with Furby name...');
    userDisconnect = false;
    var dev, entry;
    try {
        dev = await navigator.bluetooth.requestDevice({
            filters: [{ name: 'Furby'}], 
            optionalServices: ['generic_access', 'device_information', fluff_service]});
    } catch (e) {
        log(e.message);
        return;
    }
    if (furbies.some(f => f.dev.id === dev.id)) { msg.error('That Furby is already connected'); return; }
    entry = { id: ++furbyCount, dev: dev, chars: {}, name: 'Furby ' + furbyCount, ready: false };
    dev.addEventListener('gattserverdisconnected', () => onOneDisconnected(entry));
    msg.ok('Connecting ' + entry.name + '...');
    try {
        await setupFurby(entry);
    } catch (e) {
        msg.error('Could not connect ' + entry.name + ', try again');
        log(e.message);
        furbyCount--;
        return;
    }
    device = dev;
    entry.ready = true;
    if (furbies.length == 0) furby_chars = entry.chars;

    //await triggerAction(39,4,2,0); // 'get ready'
    var first = furbies.length == 0;
    furbies.push(entry);
    if (first) {
        gp_listen_callbacks = [];
        onConnected();
        addGPListenCallback([0x21], onFurbySensorData);
    } else {
        msg.ok(entry.name + ' connected!');
    }
    updateTargets();
    try {
        if (first) await checkActiveSlot();
    } catch (e) {
        log('Could not read active DLC: ' + e.message);
    }
    // flash this Furby's antenna so you can tell which one just connected
    for (const c of [[255,0,0],[0,255,0],[0,0,255]]) {
        await writeTo(entry, [0x14].concat(c)).catch(() => {});
        await sleep(600);
    }
}
function doAddFurby() { return doConnect(); }

// Runs fn once for each selected Furby, one Furby at a time, so a DLC
// upload goes to every chosen Furby without them getting in each other's way.
async function forEachTarget(fn) {
    var list = targets().slice();
    if (list.length == 0) { msg.error('No Furby connected'); return; }
    var prevTarget = sendTarget;
    try {
        for (var i = 0; i < list.length; i++) {
            var f = list[i];
            if (!f.ready) continue;
            sendTarget = f.id;
            furby_chars = f.chars;
            if (list.length > 1) log('--- ' + f.name + ' (' + (i + 1) + ' of ' + list.length + ') ---');
            await fn(f, i, list.length);
        }
    } finally {
        sendTarget = prevTarget;
        if (furbies.length) furby_chars = furbies[0].chars;
        updateTargets();
    }
}
function setSendTarget(v) { sendTarget = v; }
