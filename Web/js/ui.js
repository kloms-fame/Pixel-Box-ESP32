const UIManager = {
    els: {},
    scannedMacs: new Set(),

    init() {
        // 缓存常用 DOM 节点
        this.els.log = document.getElementById('sysLog');
        this.els.radarList = document.getElementById('radarList');
        this.els.alarmContainer = document.getElementById('alarmContainer');
        this.els.btnConnect = document.getElementById('btnConnect');
        this.els.btnDisconnect = document.getElementById('btnDisconnect');

        // 动态生成 3 组闹钟 UI 模板
        this.els.alarmContainer.innerHTML = '';
        for (let i = 0; i < 3; i++) {
            this.els.alarmContainer.innerHTML += `
        <div class="list-item" style="background:#000; padding:12px; border-radius:4px; margin-bottom:8px; border:1px solid var(--border);">
          <div style="display:flex; flex-direction:column; gap:8px;">
            <div class="list-item-title">ALM_NODE_${i + 1}</div>
            <input type="time" id="almTime${i}" class="ctrl-btn" disabled>
          </div>
          <div style="display: flex; gap: 12px; align-items: center;">
            <label class="toggle-switch">
              <input type="checkbox" id="almEn${i}" class="ctrl-btn" disabled>
              <span class="slider-round"></span>
            </label>
            <button onclick="AppController.setAlarm(${i})" class="btn-success ctrl-btn" disabled style="flex-grow:0; padding:6px 12px;">WRITE</button>
          </div>
        </div>`;
        }

        // 生成完毕后统一抓取所有受控元素
        this.els.ctrlBtns = document.querySelectorAll('.ctrl-btn');
    },

    log(msg) {
        const time = new Date().toLocaleTimeString('en-US', { hour12: false });
        this.els.log.innerHTML += `\n> ${time} | ${msg}`;
        this.els.log.scrollTop = this.els.log.scrollHeight;
    },

    setConnectState(isConnected) {
        this.els.btnConnect.disabled = isConnected;
        this.els.btnDisconnect.disabled = !isConnected;
        this.els.ctrlBtns.forEach(btn => btn.disabled = !isConnected);
    },

    clearRadar() {
        this.scannedMacs.clear();
        this.els.radarList.innerHTML = '<div style="color:var(--accent); text-align:center; padding:10px;">// SCANNING FREQUENCIES...</div>';
    },

    addRadarDevice(type, addrType, macStr, name) {
        if (this.scannedMacs.has(macStr)) return;
        this.scannedMacs.add(macStr);

        if (this.els.radarList.innerText.includes('SCANNING')) this.els.radarList.innerHTML = '';

        let typeStr = type === 1 ? 'HRM [心率]' : type === 2 ? 'CSC [踏频]' : 'UNK [未知]';
        let typeColor = type !== 0 ? 'var(--accent)' : 'var(--text-muted)';

        const item = document.createElement('div');
        item.className = 'list-item';
        item.innerHTML = `
      <div>
        <div class="list-item-title">${name || 'UNKNOWN_NODE'}</div>
        <div class="list-item-sub" style="color: ${typeColor};">${typeStr} | ${macStr}</div>
      </div>
      <div style="display:flex; flex-direction:column; gap:4px;">
        <button onclick="AppController.bindDevice(${addrType}, '${macStr}')" style="padding:4px 12px;">BIND</button>
        <button class="btn-danger" onclick="AppController.dropDevice(${addrType}, '${macStr}')" style="padding:4px 12px;">DROP</button>
      </div>
    `;
        this.els.radarList.appendChild(item);
    }
};