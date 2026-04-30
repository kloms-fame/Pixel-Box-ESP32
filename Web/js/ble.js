const BLEManager = {
    UUID_SVC: "6e400001-b5a3-f393-e0a9-e50e24dcca9e",
    UUID_RX: "6e400003-b5a3-f393-e0a9-e50e24dcca9e",
    UUID_TX: "6e400004-b5a3-f393-e0a9-e50e24dcca9e",
    device: null,
    rxChar: null,
    txChar: null,

    async connect(onRxCallback, onDisconnectCallback) {
        this.device = await navigator.bluetooth.requestDevice({ filters: [{ services: [this.UUID_SVC] }] });
        const server = await this.device.gatt.connect();
        const service = await server.getPrimaryService(this.UUID_SVC);

        this.rxChar = await service.getCharacteristic(this.UUID_RX);
        this.txChar = await service.getCharacteristic(this.UUID_TX);

        await this.txChar.startNotifications();
        this.txChar.addEventListener('characteristicvaluechanged', onRxCallback);
        this.device.addEventListener('gattserverdisconnected', onDisconnectCallback);

        return this.device.name;
    },

    async disconnect() {
        if (this.device && this.device.gatt.connected) {
            await this.device.gatt.disconnect();
        }
    },

    async sendCmd(cmdArray) {
        if (!this.rxChar) return;
        await this.rxChar.writeValue(new Uint8Array(cmdArray));
    }
};