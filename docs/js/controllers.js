class SystemController {
    constructor() {
        this.slBright = document.getElementById('slBrightness');
        this.chkAutoRec = document.getElementById('chkAutoRec');
        this.slBright.onchange = (e) => { BLEManager.send([0x01, parseInt(e.target.value)]); UI.log(`[SYS] 亮度 -> ${e.target.value}`); };
        this.chkAutoRec.onchange = (e) => { BLEManager.send([0x13, e.target.checked ? 1 : 0]); UI.log(`[SYS] 自动重连 -> ${e.target.checked}`); };
        document.getElementById('btnScan').onclick = () => { UI.clearRadar(); BLEManager.send([0x05]); UI.log("[RDR] 发起物理层扫描..."); };
    }
    syncState(b, a) { this.slBright.value = b; this.chkAutoRec.checked = (a === 1); UI.log("[SYNC] 基础配置已同步"); }
}

class AlarmController {
    constructor() {
        const container = document.getElementById('alarmContainer');
        for (let i = 0; i < 3; i++) {
            container.innerHTML += `
            <div class="list-item" style="background:#000; padding:12px; border-radius:4px; margin-bottom:8px; border:1px solid var(--border);">
              <div style="display:flex; flex-direction:column; gap:8px;">
                <span style="font-weight:bold; color:var(--text-main);">ALM_NODE_${i + 1}</span>
                <input type="time" id="almTime${i}" class="ctrl-node" disabled>
              </div>
              <div style="display:flex; gap:12px; align-items:center;">
                <label class="toggle-switch">
                  <input type="checkbox" id="almEn${i}" class="ctrl-node" disabled>
                  <span class="slider-round"></span>
                </label>
                <button id="almBtn${i}" class="btn-success ctrl-node" disabled style="padding:6px 12px;">写入</button>
              </div>
            </div>`;
        }
        for (let i = 0; i < 3; i++) document.getElementById(`almBtn${i}`).onclick = () => {
            const t = document.getElementById(`almTime${i}`).value;
            const en = document.getElementById(`almEn${i}`).checked ? 1 : 0;
            if (!t) return;
            const [h, m] = t.split(':').map(Number);
            BLEManager.send([0x0F, i, en, h, m]); UI.log(`[ALM] 写入节点 ${i + 1} -> ${t}`);
        };
    }
    syncState(idx, isSet, enabled, h, m) {
        if (isSet) {
            document.getElementById(`almTime${idx}`).value = `${String(h).padStart(2, '0')}:${String(m).padStart(2, '0')}`;
            document.getElementById(`almEn${idx}`).checked = (enabled === 1);
        }
    }
}

class TimeController {
    constructor() {
        document.getElementById('btnUiClock').onclick = () => {
            const d = new Date(); const ts = Math.floor((d.getTime() - d.getTimezoneOffset() * 60000) / 1000);
            BLEManager.send([0x04, (ts >> 24) & 0xFF, (ts >> 16) & 0xFF, (ts >> 8) & 0xFF, ts & 0xFF]);
        };
        document.getElementById('btnUiSensor').onclick = () => BLEManager.send([0x08]);
        document.getElementById('btnUiTimer').onclick = () => BLEManager.send([0x09]);
        document.getElementById('btnUiCdown').onclick = () => BLEManager.send([0x0B]);
        document.getElementById('btnUiAlarm').onclick = () => BLEManager.send([0x0E]);
        document.getElementById('btnUiOff').onclick = () => BLEManager.send([0x03]);

        const slCd = document.getElementById('slCdown');
        slCd.oninput = (e) => document.getElementById('valCdown').innerText = e.target.value + 'm';
        document.getElementById('btnCdApply').onclick = () => BLEManager.send([0x0C, parseInt(slCd.value)]);
        document.getElementById('btnCdStart').onclick = () => BLEManager.send([0x0D, 0x01]);
        document.getElementById('btnCdPause').onclick = () => BLEManager.send([0x0D, 0x00]);
        document.getElementById('btnCdReset').onclick = () => BLEManager.send([0x0D, 0x02]);

        document.getElementById('btnTmStart').onclick = () => BLEManager.send([0x0A, 0x01]);
        document.getElementById('btnTmPause').onclick = () => BLEManager.send([0x0A, 0x00]);
        document.getElementById('btnTmReset').onclick = () => BLEManager.send([0x0A, 0x02]);

        // 【新增】接收单片机硬件按键发来的倒计时更新
        syncCdown(mins) {
            const slCd = document.getElementById('slCdown');
            const valCd = document.getElementById('valCdown');
            slCd.value = mins;
            valCd.innerText = mins + ' 分钟';
            UI.log(`[SYNC] 硬件按键下发 -> 倒数预设已变更为 ${mins} 分钟`);
        }
        // 【新增】NVS 管理控制器
        class NvsController {
            updateDevice(type, mac) {
                UI.updateNvsList(type, mac);
            }
        }
    }
}