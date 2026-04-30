class PixelApp {
    constructor() {
        this.initPWA();
        this.initDOM();
        this.bindEvents();
    }

    initPWA() {
        // 1. 注册 Service Worker 实现断网离线访问
        if ('serviceWorker' in navigator) {
            navigator.serviceWorker.register('sw.js').then(() => {
                document.getElementById('pwaBadge').style.display = 'inline-block';
                console.log("[PWA] 离线引擎注册成功");
            });
        }

        // 2. 侦测是否运行在“桌面 App 模式” (如果已经是 App，就不需要再提示安装了)
        const isInStandaloneMode = () => window.matchMedia('(display-mode: standalone)').matches || window.navigator.standalone || document.referrer.includes('android-app://');

        if (isInStandaloneMode()) {
            UI.log("[SYS] 当前运行在原生沉浸模式");
            return;
        }

        // 3. 针对 Android / Windows Chrome 的自动安装拦截
        let deferredPrompt;
        window.addEventListener('beforeinstallprompt', (e) => {
            // 阻止浏览器默认的丑陋横幅
            e.preventDefault();
            deferredPrompt = e;

            // 显示我们极简风的安装横幅
            const installBanner = document.getElementById('pwaInstallBanner');
            installBanner.style.display = 'flex';

            document.getElementById('btnInstallPWA').onclick = async () => {
                installBanner.style.display = 'none';
                deferredPrompt.prompt();
                const { outcome } = await deferredPrompt.userChoice;
                if (outcome === 'accepted') {
                    UI.log("[PWA] 应用安装成功，请去桌面查看");
                }
                deferredPrompt = null;
            };
        });

        // 4. 针对 iOS Safari 的专属拦截提醒 (苹果系统封闭，必须人工干预)
        const isIos = () => {
            const userAgent = window.navigator.userAgent.toLowerCase();
            return /iphone|ipad|ipod/.test(userAgent);
        };

        if (isIos() && !isInStandaloneMode()) {
            document.getElementById('iosInstallHint').style.display = 'block';
        }
    }

    initDOM() {
        // 初始化时注入闹钟DOM，确保后续 ID 获取绝不会报错
        const container = document.getElementById('alarmContainer');
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

    bindEvents() {
        // ====== 蓝牙系统控制 ======
        document.getElementById('btnConnect').onclick = async () => {
            try {
                UI.log("NEGOTIATING HANDSHAKE...");
                const name = await BLEManager.connect(this.handleRx.bind(this), this.handleDisconnect.bind(this));

                UI.setConnectState(true);
                document.getElementById('btnConnect').style.display = 'none';

                const btnDisc = document.getElementById('btnDisconnect');
                btnDisc.style.display = 'block';
                btnDisc.disabled = false; // 强行解除禁用状态，避免被 setConnectState 误杀

                UI.log(`LINK_ESTABLISHED: ${name}，请求 NVS 快照...`);
                // 拉取 NVS 快照
                setTimeout(() => BLEManager.sendCmd([0x10]), 500);

            } catch (e) { UI.log(`ERR_CONNECT: ${e.message}`); }
        };

        document.getElementById('btnDisconnect').onclick = () => BLEManager.disconnect();

        // ====== 系统参数 (亮度 & 自动重连) ======
        const brightSlider = document.getElementById('brightSlider');
        brightSlider.oninput = (e) => document.getElementById('brightVal').innerText = e.target.value;
        brightSlider.onchange = (e) => {
            BLEManager.sendCmd([0x01, parseInt(e.target.value)]);
            UI.log(`CMD: BRIGHTNESS_SET_TO -> ${e.target.value}`);
        };

        document.getElementById('autoRecEn').onchange = (e) => {
            BLEManager.sendCmd([0x13, e.target.checked ? 1 : 0]);
            UI.log(`CMD: AUTO_RECONNECT -> ${e.target.checked ? 'ENABLED' : 'DISABLED'}`);
        };

        // ====== UI 路由 ======
        document.getElementById('syncTimeBtn').onclick = () => {
            const d = new Date(); const ts = Math.floor((d.getTime() - d.getTimezoneOffset() * 60000) / 1000);
            BLEManager.sendCmd([0x04, (ts >> 24) & 0xFF, (ts >> 16) & 0xFF, (ts >> 8) & 0xFF, ts & 0xFF]);
            UI.log("CMD: SWAP_UI -> CLOCK_MODE");
        };
        document.getElementById('showDataBtn').onclick = () => { BLEManager.sendCmd([0x08]); UI.log("CMD: SWAP_UI -> SENSOR_DATA"); };
        document.getElementById('timerModeBtn').onclick = () => { BLEManager.sendCmd([0x09]); UI.log("CMD: SWAP_UI -> STOPWATCH"); };
        document.getElementById('countdownModeBtn').onclick = () => { BLEManager.sendCmd([0x0B]); UI.log("CMD: SWAP_UI -> COUNTDOWN"); };
        document.getElementById('alarmModeBtn').onclick = () => { BLEManager.sendCmd([0x0E]); UI.log("CMD: SWAP_UI -> ALARM_MATRIX"); };
        document.getElementById('clearBtn').onclick = () => { BLEManager.sendCmd([0x03]); UI.log("CMD: SWAP_UI -> SCREEN_OFF"); };

        // ====== 传感器雷达 ======
        document.getElementById('scanBtn').onclick = () => {
            UI.clearRadar();
            BLEManager.sendCmd([0x05]);
            UI.log("CMD: FULL_PROXY_SCAN_DISPATCHED");
        };

        // ====== 闹钟写入绑定 ======
        for (let i = 0; i < 3; i++) {
            document.getElementById(`almBtn${i}`).onclick = () => {
                const timeVal = document.getElementById(`almTime${i}`).value;
                const enabled = document.getElementById(`almEn${i}`).checked ? 1 : 0;
                if (!timeVal) { UI.log(`ERR: 第 ${i + 1} 组未配置时间!`); return; }
                const [h, m] = timeVal.split(':').map(Number);
                BLEManager.sendCmd([0x0F, i, enabled, h, m]);
                UI.log(`CMD: ALARM_SET -> IDX:${i + 1} TIME:${timeVal} EN:${enabled}`);
            };
        }

        // ====== 秒表 ======
        document.getElementById('timerStartBtn').onclick = () => { BLEManager.sendCmd([0x0A, 0x01]); UI.log("CMD: STOPWATCH -> RUN"); };
        document.getElementById('timerPauseBtn').onclick = () => { BLEManager.sendCmd([0x0A, 0x00]); UI.log("CMD: STOPWATCH -> PAUSE"); };
        document.getElementById('timerResetBtn').onclick = () => { BLEManager.sendCmd([0x0A, 0x02]); UI.log("CMD: STOPWATCH -> RESET"); };

        // ====== 倒计时 ======
        const cdSlider = document.getElementById('cdSlider');
        cdSlider.oninput = (e) => document.getElementById('cdVal').innerText = e.target.value + ' 分钟';
        document.getElementById('cdApplyBtn').onclick = () => { BLEManager.sendCmd([0x0C, parseInt(cdSlider.value)]); UI.log(`CMD: CDOWN_CFG -> ${cdSlider.value} MINS`); };
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

    // 接收并解析来自 ESP32 的所有报文 (包含握手同步报文)
    handleRx(e) {
        const v = new Uint8Array(e.target.value.buffer);
        const cmd = v[0];

        if (cmd === 0x05) {
            // 扫描结果返回
            const type = v[1]; const addrType = v[2]; const macLen = v[3];
            const macStr = new TextDecoder().decode(v.slice(4, 4 + macLen));
            const name = new TextDecoder().decode(v.slice(4 + macLen));
            UI.addRadarDevice(type, addrType, macStr, name);
        }
        else if (cmd === 0x10) {
            // 基础配置 NVS 同步
            const bright = v[2];
            const autoRec = v[3];
            document.getElementById('brightSlider').value = bright;
            document.getElementById('brightVal').innerText = bright;
            document.getElementById('autoRecEn').checked = (autoRec === 1);
            UI.log("[SYNC] 基础配置已同步至 Web");
        }
        else if (cmd === 0x11) {
            // 闹钟矩阵 NVS 同步
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
    }

    // 暴露给 HTML 行内点击事件的 API
    async bindDevice(addrType, macStr, type) {
        const payload = [0x06, addrType];
        for (let i = 0; i < macStr.length; i++) payload.push(macStr.charCodeAt(i));
        await BLEManager.sendCmd(payload);

        // 发送记忆指令给 NVS
        setTimeout(() => {
            const memPayload = [0x14, type];
            for (let i = 0; i < macStr.length; i++) memPayload.push(macStr.charCodeAt(i));
            BLEManager.sendCmd(memPayload);
        }, 300);

        UI.log(`CMD: BINDING_ISSUED -> [${macStr}]`);
    }

    async dropDevice(addrType, macStr) {
        const payload = [0x07, addrType];
        for (let i = 0; i < macStr.length; i++) payload.push(macStr.charCodeAt(i));
        await BLEManager.sendCmd(payload);
        UI.log(`CMD: DROP_ISSUED -> [${macStr}]`);
    }
}

// 挂载全局启动
window.onload = () => { window.AppController = new PixelApp(); };