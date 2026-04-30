class SystemController {
    constructor() {
        this.slBright = document.getElementById('slBrightness');
        this.chkAutoRec = document.getElementById('chkAutoRec');
        if (this.slBright) this.slBright.onchange = (e) => { BLEManager.send([0x01, parseInt(e.target.value)]); UI.log(`[SYS] 亮度 -> ${e.target.value}`); };
        if (this.chkAutoRec) this.chkAutoRec.onchange = (e) => { BLEManager.send([0x13, e.target.checked ? 1 : 0]); UI.log(`[SYS] 自动重连 -> ${e.target.checked}`); };

        const btnScan = document.getElementById('btnScan');
        if (btnScan) btnScan.onclick = () => { UI.clearRadar(); BLEManager.send([0x05]); UI.log("[RDR] 发起物理层扫描..."); };
    }
    syncState(b, a) { if (this.slBright) this.slBright.value = b; if (this.chkAutoRec) this.chkAutoRec.checked = (a === 1); UI.log("[SYNC] 基础配置已同步"); }
}

class AlarmController {
    // 预留结构，目前逻辑已在 app.js 接管
}

class TimeController {
    constructor() {
        // 预留结构
    }
    // 🌟 修复：将其从 constructor 内部提到了外部正常的类方法区
    syncCdown(mins) {
        const slCd = document.getElementById('slCdown');
        const valCd = document.getElementById('valCdown');
        if (slCd && valCd) {
            slCd.value = mins;
            valCd.innerText = mins + ' 分钟';
            UI.log(`[SYNC] 硬件按键下发 -> 倒数预设已变更为 ${mins} 分钟`);
        }
    }
}

// 🌟 修复：将 NvsController 拆分出来
class NvsController {
    updateDevice(type, mac) {
        UI.updateNvsList(type, mac);
    }
}