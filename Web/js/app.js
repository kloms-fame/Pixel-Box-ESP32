const AppController = {
    init() {
        UIManager.init();
        this.bindEvents();
        UIManager.log("SYSTEM_READY: Waiting for BLE handshake.");
    },

    bindEvents() {
        // ---- 系统连接路由 ----
        document.getElementById('btnConnect').onclick = async () => {
            try {
                UIManager.log("NEGOTIATING HANDSHAKE...");
                // 传入两个回调：收到数据、断开连接
                const name = await BLEManager.connect(this.handleGatewayRx, this.handleDisconnect);
                UIManager.setConnectState(true);
                UIManager.log(`LINK_ESTABLISHED: ${name}`);
            } catch (e) {
                UIManager.log(`ERR_CONNECT: ${e.message}`);
            }
        };
        document.getElementById('btnDisconnect').onclick = () => BLEManager.disconnect();

        // ---- UI 切屏控制路由 ----
        document.getElementById('btnUiClock').onclick = () => {
            const now = new Date();
            const ts = Math.floor((now.getTime() - now.getTimezoneOffset() * 60000) / 1000);
            BLEManager.sendCmd([0x04, (ts >> 24) & 0xFF, (ts >> 16) & 0xFF, (ts >> 8) & 0xFF, ts & 0xFF]);
            UIManager.log("CMD: SWAP_UI -> CLOCK_MODE");
        };
        document.getElementById('btnUiSensor').onclick = () => { BLEManager.sendCmd([0x08]); UIManager.log("CMD: SWAP_UI -> SENSOR"); };
        document.getElementById('btnUiTimer').onclick = () => { BLEManager.sendCmd([0x09]); UIManager.log("CMD: SWAP_UI -> STOPWATCH"); };
        document.getElementById('btnUiCdown').onclick = () => { BLEManager.sendCmd([0x0B]); UIManager.log("CMD: SWAP_UI -> COUNTDOWN"); };
        document.getElementById('btnUiAlarm').onclick = () => { BLEManager.sendCmd([0x0E]); UIManager.log("CMD: SWAP_UI -> ALARM_MATRIX"); };
        document.getElementById('btnUiOff').onclick = () => { BLEManager.sendCmd([0x03]); UIManager.log("CMD: SWAP_UI -> SCREEN_OFF"); };

        // ---- 亮度滑动条路由 ----
        const slBright = document.getElementById('slBrightness');
        const valBright = document.getElementById('valBrightness');
        slBright.oninput = (e) => valBright.innerText = e.target.value;
        slBright.onchange = (e) => {
            BLEManager.sendCmd([0x01, parseInt(e.target.value)]);
            UIManager.log(`CMD: BRIGHTNESS -> ${e.target.value}`);
        };

        // ---- 雷达探测路由 ----
        document.getElementById('btnScan').onclick = () => {
            UIManager.clearRadar();
            BLEManager.sendCmd([0x05]);
            UIManager.log("CMD: PROXY_SCAN_DISPATCHED");
        };

        // ---- 秒表时间序列路由 ----
        document.getElementById('btnTmStart').onclick = () => { BLEManager.sendCmd([0x0A, 0x01]); UIManager.log("CMD: STOPWATCH -> RUN"); };
        document.getElementById('btnTmPause').onclick = () => { BLEManager.sendCmd([0x0A, 0x00]); UIManager.log("CMD: STOPWATCH -> PAUSE"); };
        document.getElementById('btnTmReset').onclick = () => { BLEManager.sendCmd([0x0A, 0x02]); UIManager.log("CMD: STOPWATCH -> RESET"); };

        // ---- 倒计时引擎路由 ----
        const slCd = document.getElementById('slCdown');
        const valCd = document.getElementById('valCdown');
        slCd.oninput = (e) => valCd.innerText = e.target.value + ' Min';
        document.getElementById('btnCdApply').onclick = () => { BLEManager.sendCmd([0x0C, parseInt(slCd.value)]); UIManager.log(`CMD: CDOWN_CFG -> ${slCd.value} MINS`); };
        document.getElementById('btnCdStart').onclick = () => { BLEManager.sendCmd([0x0D, 0x01]); UIManager.log("CMD: CDOWN -> RUN"); };
        document.getElementById('btnCdPause').onclick = () => { BLEManager.sendCmd([0x0D, 0x00]); UIManager.log("CMD: CDOWN -> PAUSE"); };
        document.getElementById('btnCdReset').onclick = () => { BLEManager.sendCmd([0x0D, 0x02]); UIManager.log("CMD: CDOWN -> RESET"); };
    },

    // ---- 接收单片机回传报文 ----
    handleGatewayRx(e) {
        const v = new Uint8Array(e.target.value.buffer);
        if (v[0] === 0x05) { // 扫描报文 0x05 解析
            const type = v[1]; const addrType = v[2]; const macLen = v[3];
            const macStr = new TextDecoder().decode(v.slice(4, 4 + macLen));
            const name = new TextDecoder().decode(v.slice(4 + macLen));
            UIManager.addRadarDevice(type, addrType, macStr, name);
        }
    },

    // ---- 异常掉线处理 ----
    handleDisconnect() {
        UIManager.setConnectState(false);
        UIManager.log("LINK_TERMINATED: Gateway offline");
    },

    // ---- 对外暴露 API (供 HTML 行内 onclick 调用) ----
    async bindDevice(addrType, macStr) {
        const payload = [0x06, addrType];
        for (let i = 0; i < macStr.length; i++) payload.push(macStr.charCodeAt(i));
        await BLEManager.sendCmd(payload);
        UIManager.log(`CMD: BIND_SENSOR -> [${macStr}]`);
    },

    async dropDevice(addrType, macStr) {
        const payload = [0x07, addrType];
        for (let i = 0; i < macStr.length; i++) payload.push(macStr.charCodeAt(i));
        await BLEManager.sendCmd(payload);
        UIManager.log(`CMD: DROP_SENSOR -> [${macStr}]`);
    },

    async setAlarm(idx) {
        const timeVal = document.getElementById(`almTime${idx}`).value;
        const enabled = document.getElementById(`almEn${idx}`).checked ? 1 : 0;
        if (!timeVal) { UIManager.log(`ERR: ALM_NODE_${idx + 1} lacks timestamp`); return; }
        const [h, m] = timeVal.split(':').map(Number);
        await BLEManager.sendCmd([0x0F, idx, enabled, h, m]);
        UIManager.log(`CMD: ALARM_SET -> IDX:${idx + 1} TIME:${timeVal} EN:${enabled}`);
    }
};

// 挂载全局环境并启动
window.AppController = AppController;
window.onload = () => AppController.init();