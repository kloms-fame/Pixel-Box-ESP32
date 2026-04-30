const UI = {
  logEl: document.getElementById('sysLog'),
  radarEl: document.getElementById('radarList'),
  nvsEl: document.getElementById('nvsDeviceList'),
  scannedMacs: new Set(),

  log(msg) {
    if (!this.logEl) return;
    const time = new Date().toLocaleTimeString('en-US', { hour12: false });
    this.logEl.innerHTML += `\n> ${time} | ${msg}`;
    this.logEl.scrollTop = this.logEl.scrollHeight;
  },

  setConnectState(isConnected) {
    document.querySelectorAll('.ctrl-btn').forEach(btn => btn.disabled = !isConnected);
  },

  clearRadar() {
    this.scannedMacs.clear();
    // 统一下发包含 SCANNING 关键字的 HTML，方便后续匹配删除
    if (this.radarEl) this.radarEl.innerHTML = '<div style="color:var(--accent); text-align:center; padding:10px;">// SCANNING FREQUENCIES...</div>';
  },

  addRadarDevice(type, addrType, macStr, name) {
    if (!this.radarEl) return;
    if (this.scannedMacs.has(macStr)) return;
    this.scannedMacs.add(macStr);

    // 【完美修正】：只要列表里有占位符文本，就清空列表
    if (this.radarEl.innerText.includes('SCANNING') || this.radarEl.innerText.includes('AWAITING')) {
      this.radarEl.innerHTML = '';
    }

    let typeStr = type === 1 ? 'HRM - 心率监测' : type === 2 ? 'CSC - 速度踏频' : 'UNK - 未知设备';
    let typeColor = type !== 0 ? 'var(--accent)' : '#52525b';

    const item = document.createElement('div');
    item.className = 'list-item';
    item.innerHTML = `
      <div style="display: flex; flex-direction: column; gap: 4px;">
        <span style="color: #f4f4f5;">${name || 'UNKNOWN_NODE'}</span>
        <span style="font-size: 10px; color: ${typeColor};">${typeStr} [${macStr}]</span>
      </div>
      <div>
        <button onclick="window.AppTools.bindDevice(${addrType}, '${macStr}', ${type})" style="margin:0; padding:5px 10px;">CONNECT</button>
        <button class="btn-danger" onclick="window.AppTools.dropDevice(${addrType}, '${macStr}')" style="margin:0; padding:5px 10px; margin-left:4px;">DROP</button>
      </div>`;
    this.radarEl.appendChild(item);
  },

  updateNvsList(type, macStr) {
    if (!this.nvsEl) return;

    if (!macStr) {
      const existing = document.getElementById(`nvs-item-${type}`);
      if (existing) existing.remove();
      if (this.nvsEl.children.length === 0) {
        this.nvsEl.innerHTML = '<div class="standby-text" style="color:var(--text-muted); text-align:center; margin-top:20px;">// NO SAVED DEVICES</div>';
      }
      return;
    }

    if (this.nvsEl.innerText.includes('NO SAVED')) this.nvsEl.innerHTML = '';

    let item = document.getElementById(`nvs-item-${type}`);
    if (!item) {
      item = document.createElement('div');
      item.id = `nvs-item-${type}`;
      item.className = 'list-item';
      item.style.cssText = "background:#000; padding:12px; border-radius:4px; margin-bottom:8px; border:1px solid var(--border);";
      this.nvsEl.appendChild(item);
    }

    let typeStr = type === 1 ? 'HRM - 记忆心率' : 'CSC - 记忆踏频';
    item.innerHTML = `
      <div style="display: flex; flex-direction: column; gap: 4px;">
        <span style="font-weight:bold; color:var(--text-main);">${typeStr}</span>
        <span style="font-size: 10px; color: var(--accent);">${macStr}</span>
      </div>
      <div>
        <button onclick="window.AppTools.forgetDevice(${type})" class="btn-danger" style="margin:0; padding:5px 10px;">FORGET</button>
      </div>`;
  }
};