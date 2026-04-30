class PixelApp {
    constructor() {
        this.initPWA();
        this.initDOM();
        this.bindEvents();
    }

    initPWA() {
        if ('serviceWorker' in navigator) {
            navigator.serviceWorker.register('sw.js').then(() => {
                document.getElementById('pwaBadge').style.display = 'inline-block';
            });
        }

        const isInStandaloneMode = () => window.matchMedia('(display-mode: standalone)').matches || window.navigator.standalone || document.referrer.includes('android-app://');

        if (isInStandaloneMode()) {
            UI.log("[SYS] 当前运行在原生沉浸模式");
            return;
        }

        let deferredPrompt;
        window.addEventListener('beforeinstallprompt', (e) => {
            e.preventDefault();
            deferredPrompt = e;
            const installBanner = document.getElementById('pwaInstallBanner');
            if (installBanner) installBanner.style.display = 'flex';

            const btnInstall = document.getElementById('btnInstallPWA');
            if (btnInstall) {
                btnInstall.onclick = async () => {
                    installBanner.style.display = 'none';
                    deferredPrompt.prompt();
                    const { outcome } = await deferredPrompt.userChoice;
                    if (outcome === 'accepted') UI.log("[PWA] 应用安装成功，请去桌面查看");
                    deferredPrompt = null;
                };
            }
        });

        const isIos = () => /iphone|ipad|ipod/.test(window.navigator.userAgent.toLowerCase());
        if (isIos() && !isInStandaloneMode()) {
            const iosHint = document.getElementById('iosInstallHint');
            if (iosHint) iosHint.style.display = 'block';
        }
    }

    initDOM() {
        const container = document.getElementById('alarmContainer');
        if (container) {
            container.innerHTML = '';
            for (let i = 0; i < 3; i++) {
                container.innerHTML += `
          <div class="list-item" style="background:#000; padding:12px; border-radius:4px; margin-bottom:8px; border:1px solid var(--border);">
            <div style="display:flex; flex-direction:column; gap:8px;">
              <span style="font-weight:bold; color:var(--text-main);">ALM_NODE_${i + 1}</span>
              <input type="time" id="almTime${i}" class="ctrl-btn" disabled>
            </div>
            <div style="display: flex; gap: 12px; align-items: center;">
              <label class="toggle-switch">
                <input type="checkbox" id="almEn${i}" class="ctrl-btn" disabled>
                <span class="slider-round"></span>
              </label>
              <button id="almBtn${i}" class="btn-success ctrl-btn" disabled style="margin: 0; flex-grow:0; padding:6px 12px;">注入配置</button>
            </div>
          </div>`;
            }
        }
    }

    bindEvents() {
        const brandTitle = document.getElementById('brandTitle');
        if (brandTitle) {
            let clickCount = 0;
            let clickTimer = null;
            brandTitle.onclick = () => {
                clickCount++;
                if (clickCount === 1) {
                    clickTimer = setTimeout(() => { clickCount = 0; }, 2000);
                } else if (clickCount >= 5) {
                    clearTimeout(clickTimer);
                    clickCount = 0;
                    if (confirm("🧨 危险操作警告：\n\n这将会彻底擦除设备内所有的 NVS 存储记忆（包括所有闹钟、系统亮度、自动重连以及已绑定的骑行设备），并将设备重启！\n\n您确定要将设备恢复出厂设置吗？")) {
                        BLEManager.sendCmd([0xFF]);
                        UI.log("[SYS] 已下发恢复出厂设置指令，正在清理设备...");
                        setTimeout(() => { BLEManager.disconnect(); }, 500);
                        let countdown = 3;
                        const timer = setInterval(() => {
                            UI.log(`[SYS] 设备重启中，网页将在 ${countdown} 秒后自动刷新...`);
                            countdown--;
                            if (countdown < 0) {
                                clearInterval(timer);
                                window.location.reload(true);
                            }
                        }, 1000);
                    }
                }
            };
        }

        const wifiModal = document.getElementById('wifiModal');
        const btnOpenWifi = document.getElementById('btnOpenWifi');
        if (btnOpenWifi) btnOpenWifi.onclick = () => { wifiModal.style.display = 'flex'; };
        if (document.getElementById('btnWifiClose')) document.getElementById('btnWifiClose').onclick = () => { wifiModal.style.display = 'none'; };

        if (document.getElementById('btnWifiSave')) {
            document.getElementById('btnWifiSave').onclick = async () => {
                const ssid = document.getElementById('wifiSSID').value;
                const pass = document.getElementById('wifiPass').value;
                if (!ssid) { UI.log("ERR: SSID 不能为空"); return; }
                const encoder = new TextEncoder();
                const ssidArr = encoder.encode(ssid);
                const passArr = encoder.encode(pass);
                const payload = new Uint8Array([0x20, ssidArr.length, ...ssidArr, passArr.length, ...passArr]);
                await BLEManager.sendCmd(Array.from(payload));
                UI.log(`[NET] WiFi 配置已注入单片机 NVS -> [${ssid}]`);
                wifiModal.style.display = 'none';
            };
        }

        document.getElementById('btnConnect').onclick = async () => {
            try {
                UI.log("NEGOTIATING HANDSHAKE...");
                const name = await BLEManager.connect(this.handleRx.bind(this), this.handleDisconnect.bind(this));
                UI.setConnectState(true);
                document.getElementById('btnConnect').style.display = 'none';
                const btnDisc = document.getElementById('btnDisconnect');
                btnDisc.style.display = 'block';
                btnDisc.disabled = false;
                UI.log(`LINK_ESTABLISHED: ${name}，下发系统时间并请求配置...`);

                // 🌟 核心修复2：直接发送纯净的 UTC 标准时间戳，抛弃所有时区干扰！
                const ts = Math.floor(Date.now() / 1000);
                BLEManager.sendCmd([0x04, (ts >> 24) & 0xFF, (ts >> 16) & 0xFF, (ts >> 8) & 0xFF, ts & 0xFF]);

                setTimeout(() => BLEManager.sendCmd([0x10]), 500);
                setTimeout(() => BLEManager.sendCmd([0x17]), 600);

            } catch (e) { UI.log(`ERR_CONNECT: ${e.message}`); }
        };

        document.getElementById('btnDisconnect').onclick = () => BLEManager.disconnect();

        const brightSlider = document.getElementById('brightSlider');
        if (brightSlider) {
            brightSlider.oninput = (e) => document.getElementById('brightVal').innerText = e.target.value;
            brightSlider.onchange = (e) => {
                BLEManager.sendCmd([0x01, parseInt(e.target.value)]);
                UI.log(`CMD: BRIGHTNESS_SET_TO -> ${e.target.value}`);
            };
        }

        const autoRecEn = document.getElementById('autoRecEn');
        if (autoRecEn) {
            autoRecEn.onchange = (e) => {
                BLEManager.sendCmd([0x13, e.target.checked ? 1 : 0]);
                UI.log(`CMD: AUTO_RECONNECT -> ${e.target.checked ? 'ENABLED' : 'DISABLED'}`);
            };
        }

        document.getElementById('syncTimeBtn').onclick = () => {
            // 🌟 核心修复2：此处手动同步时间按钮同样使用纯净的 UTC
            const ts = Math.floor(Date.now() / 1000);
            BLEManager.sendCmd([0x04, (ts >> 24) & 0xFF, (ts >> 16) & 0xFF, (ts >> 8) & 0xFF, ts & 0xFF]);
            UI.log("CMD: SWAP_UI -> CLOCK_MODE");
        };

        document.getElementById('showDataBtn').onclick = () => { BLEManager.sendCmd([0x08]); UI.log("CMD: SWAP_UI -> SENSOR_DATA"); };
        document.getElementById('timerModeBtn').onclick = () => { BLEManager.sendCmd([0x09]); UI.log("CMD: SWAP_UI -> STOPWATCH"); };
        document.getElementById('countdownModeBtn').onclick = () => { BLEManager.sendCmd([0x0B]); UI.log("CMD: SWAP_UI -> COUNTDOWN"); };
        document.getElementById('alarmModeBtn').onclick = () => { BLEManager.sendCmd([0x0E]); UI.log("CMD: SWAP_UI -> ALARM_MATRIX"); };
        document.getElementById('clearBtn').onclick = () => { BLEManager.sendCmd([0x03]); UI.log("CMD: SWAP_UI -> SCREEN_OFF"); };

        document.getElementById('scanBtn').onclick = () => {
            UI.clearRadar();
            BLEManager.sendCmd([0x05]);
            UI.log("CMD: FULL_PROXY_SCAN_DISPATCHED");
        };

        for (let i = 0; i < 3; i++) {
            const almBtn = document.getElementById(`almBtn${i}`);
            if (almBtn) {
                almBtn.onclick = () => {
                    const timeVal = document.getElementById(`almTime${i}`).value;
                    const enabled = document.getElementById(`almEn${i}`).checked ? 1 : 0;
                    if (!timeVal) { UI.log(`ERR: 第 ${i + 1} 组未配置时间!`); return; }
                    const [h, m] = timeVal.split(':').map(Number);
                    BLEManager.sendCmd([0x0F, i, enabled, h, m]);
                    UI.log(`CMD: ALARM_SET -> IDX:${i + 1} TIME:${timeVal} EN:${enabled}`);
                };
            }
        }

        document.getElementById('timerStartBtn').onclick = () => { BLEManager.sendCmd([0x0A, 0x01]); UI.log("CMD: STOPWATCH -> RUN"); };
        document.getElementById('timerPauseBtn').onclick = () => { BLEManager.sendCmd([0x0A, 0x00]); UI.log("CMD: STOPWATCH -> PAUSE"); };
        document.getElementById('timerResetBtn').onclick = () => { BLEManager.sendCmd([0x0A, 0x02]); UI.log("CMD: STOPWATCH -> RESET"); };

        const cdSlider = document.getElementById('cdSlider');
        if (cdSlider) {
            cdSlider.oninput = (e) => document.getElementById('cdVal').innerText = e.target.value + ' 分钟';
            document.getElementById('cdApplyBtn').onclick = () => { BLEManager.sendCmd([0x0C, parseInt(cdSlider.value)]); UI.log(`CMD: CDOWN_CFG -> ${cdSlider.value} MINS`); };
        }

        document.getElementById('cdStartBtn').onclick = () => { BLEManager.sendCmd([0x0D, 0x01]); UI.log("CMD: COUNTDOWN -> RUN"); };
        document.getElementById('cdPauseBtn').onclick = () => { BLEManager.sendCmd([0x0D, 0x00]); UI.log("CMD: COUNTDOWN -> PAUSE"); };
        document.getElementById('cdResetBtn').onclick = () => { BLEManager.sendCmd([0x0D, 0x02]); UI.log("CMD: COUNTDOWN -> RESET"); };
    }

    handleDisconnect() {
        UI.setConnectState(false);
        document.getElementById('btnConnect').style.display = 'block';
        document.getElementById('btnDisconnect').style.display = 'none';
        UI.log("LINK_TERMINATED: 设备已离线");
    }

    handleRx(e) {
        const v = new Uint8Array(e.target.value.buffer);
        const cmd = v[0];

        if (cmd === 0x05) {
            const type = v[1]; const addrType = v[2]; const macLen = v[3];
            const macStr = new TextDecoder().decode(v.slice(4, 4 + macLen));
            const name = new TextDecoder().decode(v.slice(4 + macLen));
            UI.addRadarDevice(type, addrType, macStr, name);
        }
        else if (cmd === 0x10) {
            const bright = v[2];
            const autoRec = v[3];
            document.getElementById('brightSlider').value = bright;
            document.getElementById('brightVal').innerText = bright;
            document.getElementById('autoRecEn').checked = (autoRec === 1);
            UI.log("[SYNC] 基础配置已同步至 Web");
        }
        else if (cmd === 0x11) {
            const idx = v[1];
            const isSet = v[2];
            const enabled = v[3];
            const h = v[4];
            const m = v[5];
            if (isSet === 1) {
                document.getElementById(`almTime${idx}`).value = `${String(h).padStart(2, '0')}:${String(m).padStart(2, '0')}`;
                document.getElementById(`almEn${idx}`).checked = (enabled === 1);
            }
        }
        else if (cmd === 0x12) {
            const mins = v[1];
            const slCd = document.getElementById('cdSlider');
            if (slCd) {
                slCd.value = mins;
                document.getElementById('cdVal').innerText = mins + ' 分钟';
                UI.log(`[SYNC] 硬件按键触发 -> 倒计时已变更为 ${mins} 分钟`);
            }
        }
        else if (cmd === 0x16) {
            const type = v[1];
            const macLen = v[2];
            const macStr = macLen > 0 ? new TextDecoder().decode(v.slice(3, 3 + macLen)) : "";
            UI.updateNvsList(type, macStr);
        }
        else if (cmd === 0x17) {
            const ssidLen = v[1];
            const ssid = ssidLen > 0 ? new TextDecoder().decode(v.slice(2, 2 + ssidLen)) : "";
            const inputSSID = document.getElementById('wifiSSID');
            if (inputSSID) inputSSID.value = ssid;
            UI.log(`[SYNC] 设备内置网络档案: ${ssid || '无'}`);
        }
    }
}

window.AppTools = {
    async bindDevice(addrType, macStr, type) {
        const payload = [0x06, addrType];
        for (let i = 0; i < macStr.length; i++) payload.push(macStr.charCodeAt(i));
        await BLEManager.sendCmd(payload);

        setTimeout(() => {
            const memPayload = [0x14, type];
            for (let i = 0; i < macStr.length; i++) memPayload.push(macStr.charCodeAt(i));
            BLEManager.sendCmd(memPayload);
            UI.updateNvsList(type, macStr);
        }, 1500);

        UI.log(`CMD: BINDING_ISSUED -> [${macStr}]`);
    },

    async dropDevice(addrType, macStr) {
        const payload = [0x07, addrType];
        for (let i = 0; i < macStr.length; i++) payload.push(macStr.charCodeAt(i));
        await BLEManager.sendCmd(payload);
        UI.log(`CMD: DROP_ISSUED -> [${macStr}]`);
    },

    async forgetDevice(type) {
        await BLEManager.sendCmd([0x15, type]);
        UI.log(`[SYS] 已向硬件发送擦除 NVS 请求: 节点 ${type}`);
        UI.updateNvsList(type, "");
    }
};

window.onload = () => { window.AppController = new PixelApp(); };