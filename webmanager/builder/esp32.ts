import { SerialPort, SlipDecoder, SlipEncoder } from "serialport";
import { SetOptions } from '@serialport/bindings-interface'
import * as util from 'node:util';
import * as fs from 'node:fs';

const resetAndBootToBootloader: SetOptions = { dtr: false, rts: true };
const releaseResetButKeepBootloader: SetOptions = { dtr: true, rts: false };
const freeRunningEPS32: SetOptions = { dtr: false, rts: false };
const CHIP_DETECT_MAGIC_REG_ADDR = 0x40001000;
const ESP32S3_MACFUSEADDR = 0x60007000 + 0x044;
const REQUEST = 0x00;
const RESPONSE = 0x01;
const ESP_SYNC = 0x08;
const ESP_WRITE_REG = 0x09;
const ESP_READ_REG = 0x0a;


const sleep = async (ms = 100) => new Promise((resolve) => setTimeout(resolve, ms));
const n = () => new Date().toLocaleTimeString();


class BootloaderReturn {
    constructor(public readonly valid: boolean, public readonly value: number, public readonly payload: Buffer | null) { }
}

var efuses: Array<number> = [];

function calculateChecksum(data: Buffer) {
    var checksum = 0xef;
    for (var i = 0; i < data.length; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

async function sendCommandPacketWithSingleNumberValue(functionCode: number, data: number, needsChecksum = false): Promise<BootloaderReturn> {
    var b = Buffer.allocUnsafe(4);
    b.writeUint32LE(data, 0);
    return sendCommandPacket(functionCode, b, needsChecksum);
}

async function sendCommandPacket(commandCode: number, data: Buffer, needsChecksum = false): Promise<BootloaderReturn> {
    var b = Buffer.allocUnsafe(8 + data.byteLength);
    b.writeUInt8(REQUEST, 0);
    b.writeUInt8(commandCode, 1);
    b.writeUint16LE(data.byteLength, 2);
    b.writeUint32LE(needsChecksum ? calculateChecksum(data) : 0, 4);
    data.copy(b, 8);
    while (slipDecoder.read() != null) { ; }
    slipEncoder.write(b, undefined, ((error: Error | null | undefined) => {
        console.log(`${n()} Message out ${util.format(b)} and error ${error}`);
    }));
    let timeout = 10;
    while (timeout > 0) {
        var b1: Buffer = slipDecoder.read();
        if (b1) {
            if (b1.readUInt8(0) != RESPONSE) {
                console.error("b[0]!=RESPONSE");
                return new BootloaderReturn(false, 0, null);
            }
            if (b1.readUInt8(1) != commandCode) {
                console.error("b[1]!=lastCommandCode");
                return new BootloaderReturn(false, 0, null);
            }
            var receivedSize = b1.readUInt16LE(2);
            var receivedValue = b1.readUint32LE(4);
            var receivedPayload: Buffer | null = null;
            if (receivedSize > 0) {
                receivedPayload = Buffer.allocUnsafe(receivedSize);
                b1.copy(receivedPayload, 0, 8, 8 + receivedSize);
            }
            console.log(`${n()} Message in  ${util.format(b1)}`);
            return new BootloaderReturn(true, receivedValue, receivedPayload);
        }
        await sleep(50);
        timeout--;
    }
    console.error("Timeout!")
    return new BootloaderReturn(false, 0, null);
}
const syncbuffer = Buffer.from([0x07, 0x07, 0x12, 0x20, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,]);

const syncronize = async (retries: number) => {
    while (retries > 0) {
        var res = await sendCommandPacket(ESP_SYNC, syncbuffer);
        if (res.valid) break;
        retries--;
    }
    return retries == 0 ? false : true;
};
const readRegister = (reg: number) => sendCommandPacketWithSingleNumberValue(ESP_READ_REG, reg);

const slipEncoder = new SlipEncoder({ bluetoothQuirk: true });//After analysis of the data of the original tool, I discovered, that each packet starts with a 0xC0 char (which is the package end char). the bluetoothQuirk option does exactly this...
const slipDecoder = new SlipDecoder();

function macAddr(): Array<number> {
    let macAddr = new Array(6).fill(0);
    let mac0 = efuses[0];
    let mac1 = efuses[1];
    let mac2 = efuses[2];
    let mac3 = efuses[3];
    let oui;
    //valid only for ESP32S3
    macAddr[0] = (mac1 >> 8) & 0xff;
    macAddr[1] = mac1 & 0xff;
    macAddr[2] = (mac0 >> 24) & 0xff;
    macAddr[3] = (mac0 >> 16) & 0xff;
    macAddr[4] = (mac0 >> 8) & 0xff;
    macAddr[5] = mac0 & 0xff;
    return macAddr;
}

function X02(num: number, len = 2) {
    let str = num.toString(16);
    return "0".repeat(len - str.length) + str;
}

const promisifiedOpen = (port: SerialPort) => {
    return new Promise<boolean>((resolve, reject) => {
        port.open((err) => {
            if (err) {
                resolve(false);
            }
            resolve(true);
        });
    });
}

export async function getMac(comPort: string, hostnamePrefix: string) {
    const port = new SerialPort({
        path: comPort,
        baudRate: 115200,
        autoOpen: false,
    });
    slipEncoder.pipe(port);
    port.pipe(slipDecoder);

    port.on('error', (err) => {
        console.log('Error: ', err.message)
    });

    if (!await promisifiedOpen(port)) {
        console.error("Port is not open");
        port.close();
        return;
    }
    console.log(`${n()}  Port has been opened successfully.`);
    port.set(resetAndBootToBootloader);
    await sleep(100);
    port.set(releaseResetButKeepBootloader);
    await sleep(100);
    port.set(freeRunningEPS32);

    if (!await syncronize(5)) {
        console.error("Sync was not successful");
        port.close();
        return;
    }
    console.info(`${n()} Sync was successful`);
    var res = await readRegister(CHIP_DETECT_MAGIC_REG_ADDR)
    if (res.valid) {
        console.log("The Magic Register Address is " + res.value);
    }

    let AddrMAC = ESP32S3_MACFUSEADDR;
    for (let i = 0; i < 4; i++) {
        res = await readRegister(AddrMAC + 4 * i);
        efuses[i] = res.valid ? res.value : 0;
    }
    var mac = macAddr();
    console.log(`The MAC adress is ${mac.toString()}`);
    var hostname = (`${hostnamePrefix}${X02(mac[3])}${X02(mac[4])}${X02(mac[5])}`);
    console.log(`The Hostname will be ${hostname}`);
    fs.writeFileSync("hostname.txt", hostname);
    port.close();

}