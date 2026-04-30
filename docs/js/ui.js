const UI = {
  logEl: document.getElementById('sysLog'),
  radarEl: document.getElementById('radarList'),
  scannedMacs: new Set(),

  log(msg) {
    const time = new Date().toLocaleTimeString('en-US', { hour12: false });
    this.logEl.innerHTML += `\n> ${time} | ${msg}`;
    this.logEl.scrollTop = this.logEl.scrollHeight;
  },

  setConnectState(isConnected) {
    document.querySelectorAll('.ctrl-btn').forEach(btn => btn.disabled = !isConnected);
  },

  clearRadar() {
    this.scannedMacs.clear();
    this.radarEl.innerHTML = '<div style="color:var(--accent); text-align:center; padding:10px;">// SCANNING FREQUENCIES...</div>';
  },

  addRadarDevice(type, addrType, macStr, name) {
    if (this.scannedMacs.has(macStr)) return;
    this.scannedMacs.add(macStr);

    if (this.radarEl.innerText.includes('SCANNING') || this.radarEl.innerText.includes('AWAITING')) this.radarEl.innerHTML = '';

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
        <button onclick="window.AppController.bindDevice(${addrType}, '${macStr}', ${type})" style="margin:0; padding:5px 10px;">CONNECT</button>
        <button class="btn-danger" onclick="window.AppController.dropDevice(${addrType}, '${macStr}')" style="margin:0; padding:5px 10px; margin-left:4px;">DROP</button>
      </div>`;
    this.radarEl.appendChild(item);
  }
};