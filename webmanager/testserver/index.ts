
import https from "https";
import * as fs from "node:fs";
import * as flatbuffers from 'flatbuffers';
import { WebSocketServer, WebSocket, RawData } from "ws";
import { MessageWrapper } from "./flatbuffers_gen/webmanager/message-wrapper";
import { Message } from "./flatbuffers_gen/webmanager/message";
import { ResponseSystemData } from "./flatbuffers_gen/webmanager/response-system-data";
import { PartitionInfo } from "./flatbuffers_gen/webmanager/partition-info";
import { Mac6, } from "./flatbuffers_gen/webmanager/mac6";
import { LiveLogItem } from "./flatbuffers_gen/webmanager/live-log-item";
import { ResponseWifiAccesspoints } from "./flatbuffers_gen/webmanager/response-wifi-accesspoints";
import { AccessPoint } from "./flatbuffers_gen/webmanager/access-point";
import { BooleanSetting, EnumSetting, IntegerSetting, JournalItem, RequestGetUserSettings, RequestSetUserSettings, RequestTimeseries, RequestWifiConnect, ResponseGetUserSettings, ResponseJournal, ResponseSetUserSettings, ResponseWifiConnectFailed, ResponseWifiConnectSuccessful, Setting, SettingWrapper, StringSetting, TimeGranularity } from "./flatbuffers_gen/webmanager";
import { createTimeseries } from "./timeseries_generator";


const PORT = 443;
const BUNDLED_FILE = "../dist_webui/compressed/app.html.br";

const wss = new WebSocketServer({ noServer: true });

function sendResponseSystemData(ws: WebSocket) {
    let b = new flatbuffers.Builder(1024);
    let labelOffset = b.createString("Label");
    let appnameOffset = b.createString("AppName");
    let appversionOffset = b.createString("AppVersion");
    let appdateOffset = b.createString("AppDate");
    let apptimeOffset = b.createString("AppTime");
    let piOffset = PartitionInfo.createPartitionInfo(b, labelOffset, 1, 1, 1, 1, true, appnameOffset, appversionOffset, appdateOffset, apptimeOffset);

    let piVecOffset = ResponseSystemData.createPartitionsVector(b, [piOffset]);
    ResponseSystemData.startResponseSystemData(b);
    ResponseSystemData.addChipCores(b, 2);
    ResponseSystemData.addChipFeatures(b, 255);
    ResponseSystemData.addPartitions(b, piVecOffset);
    ResponseSystemData.addChipModel(b, 2);
    ResponseSystemData.addChipRevision(b, 3);
    ResponseSystemData.addChipTemperature(b, 23.4);
    ResponseSystemData.addFreeHeap(b, 1203);
    ResponseSystemData.addMacAddressBt(b, Mac6.createMac6(b, [1, 2, 3, 4, 5, 6]));
    ResponseSystemData.addMacAddressEth(b, Mac6.createMac6(b, [1, 2, 3, 4, 5, 6]));
    ResponseSystemData.addMacAddressIeee802154(b, Mac6.createMac6(b, [1, 2, 3, 4, 5, 6]));
    ResponseSystemData.addMacAddressWifiSoftap(b, Mac6.createMac6(b, [1, 2, 3, 4, 5, 6]));
    ResponseSystemData.addMacAddressWifiSta(b, Mac6.createMac6(b, [1, 2, 3, 4, 5, 6]));
    ResponseSystemData.addSecondsEpoch(b, BigInt(Math.floor(new Date().getTime() / 1000)));
    ResponseSystemData.addSecondsUptime(b, BigInt(10));
    let rsd = ResponseSystemData.endResponseSystemData(b);
    b.finish(MessageWrapper.createMessageWrapper(b, Message.ResponseSystemData, rsd));
    ws.send(b.asUint8Array());
}

const AP_GOOD="Connectable AP";
const AP_BAD="Non connectable AP";

function sendResponseWifiAccesspoints(ws: WebSocket) {
    let b = new flatbuffers.Builder(1024);
    let ap0Offset = AccessPoint.createAccessPoint(b, b.createString(AP_GOOD), 11, -72, 2);
    let ap1Offset = AccessPoint.createAccessPoint(b, b.createString(AP_BAD), 11, -62, 2);
    let accesspointsOffset = ResponseWifiAccesspoints.createAccesspointsVector(b, [ap0Offset, ap1Offset]);
    let r = ResponseWifiAccesspoints.createResponseWifiAccesspoints(b, b.createString("MyHostnameKL"), b.createString("MySsidApKL"), accesspointsOffset);
    b.finish(MessageWrapper.createMessageWrapper(b, Message.ResponseWifiAccesspoints, r));
    ws.send(b.asUint8Array());
}

function sendResponseWifiConnectionSuccessOrFailed(ws: WebSocket, req: RequestWifiConnect){
    let b = new flatbuffers.Builder(1024);
    if(req.ssid()==AP_GOOD){
        let r = ResponseWifiConnectSuccessful.createResponseWifiConnectSuccessful(b, b.createString(AP_GOOD), 0xFF101001,0x10101002,0xFF101003, -62, b.createString("MyHostname"));
        b.finish(MessageWrapper.createMessageWrapper(b, Message.ResponseWifiConnectSuccessful, r));
    }else{
        let r = ResponseWifiConnectFailed.createResponseWifiConnectFailed(b, b.createString(AP_GOOD));
        b.finish(MessageWrapper.createMessageWrapper(b, Message.ResponseWifiConnectFailed, r));
    }
    ws.send(b.asUint8Array());
}


let toggler: boolean = false;
let counter: number = 42;

function sendResponseGetUserSettings(ws: WebSocket, req: RequestGetUserSettings) {
    var groupName= req.groupName();
    let b = new flatbuffers.Builder(1024);
    let settingsOffset: number = 0;
    if (groupName == "Group1") {
        settingsOffset = ResponseGetUserSettings.createSettingsVector(b, [
            SettingWrapper.createSettingWrapper(b, b.createString("Test String Item1"), Setting.StringSetting, StringSetting.createStringSetting(b, b.createString("Test String Item1 Value " + counter))),
            SettingWrapper.createSettingWrapper(b, b.createString("Test String Itemxyz"), Setting.StringSetting, StringSetting.createStringSetting(b, b.createString("Test String Item2 Value " + counter))),
        ]);
    }
    else if (groupName == "Group2") {
        settingsOffset = ResponseGetUserSettings.createSettingsVector(b, [
            SettingWrapper.createSettingWrapper(b, b.createString("Test String sub1item2"), Setting.StringSetting, StringSetting.createStringSetting(b, b.createString("Test String sub1item2 Value"))),
            SettingWrapper.createSettingWrapper(b, b.createString("Test Integer sub1item2"), Setting.IntegerSetting, IntegerSetting.createIntegerSetting(b, counter)),
            SettingWrapper.createSettingWrapper(b, b.createString("Test Boolean"), Setting.BooleanSetting, BooleanSetting.createBooleanSetting(b, toggler)),
            SettingWrapper.createSettingWrapper(b, b.createString("Test Enum"), Setting.EnumSetting, EnumSetting.createEnumSetting(b, counter % 4)),

        ]);
        counter++;
        toggler = !toggler;
    }

    let r = ResponseGetUserSettings.createResponseGetUserSettings(b, b.createString(groupName), settingsOffset);
    b.finish(MessageWrapper.createMessageWrapper(b, Message.ResponseGetUserSettings, r));
    ws.send(b.asUint8Array());
}

function sendResponseSetUserSettings(ws: WebSocket, req: RequestSetUserSettings) {
    let groupName = req.groupName()!;
    let names: string[] = [];
    for (let i = 0; i < req.settingsLength(); i++) {
        let name = req.settings(i)!.name()!;
        names.push(name);
    }
    console.log(`Received setting for group ${groupName} with settings ${names.join(", ")}`);
    let b = new flatbuffers.Builder(1024);
    let stringsOffset: number[] = [];
    names.forEach((v) => stringsOffset.push(b.createString(v)));
    let settingsOffset = ResponseSetUserSettings.createSettingsVector(b, stringsOffset);
    let r = ResponseSetUserSettings.createResponseSetUserSettings(b, b.createString(groupName), settingsOffset);
    b.finish(MessageWrapper.createMessageWrapper(b, Message.ResponseSetUserSettings, r));
    ws.send(b.asUint8Array());
}

function sendResponseJournal(ws: WebSocket) {
    let b = new flatbuffers.Builder(1024);
    let itemsOffset = ResponseJournal.createJournalItemsVector(b, [
        JournalItem.createJournalItem(b, 22n, b.createString("I2C_COMM"), 0, counter),
        JournalItem.createJournalItem(b, 222n, b.createString("SPI_COMM"), 0, 1),
        JournalItem.createJournalItem(b, 2222n, b.createString("I2S_COMM"), counter, 1),
        JournalItem.createJournalItem(b, 22222n, b.createString("ETH_COMM"), 0, 1),
    ]);

    counter++;

    let r = ResponseJournal.createResponseJournal(b, itemsOffset);
    b.finish(MessageWrapper.createMessageWrapper(b, Message.ResponseJournal, r));
    ws.send(b.asUint8Array());
}

function sendResponseTimeseries(ws: WebSocket, req:RequestTimeseries) {
    ws.send(createTimeseries(req.granularity()));
}


function process(buffer: Buffer, ws: WebSocket) {
    let data = new Uint8Array(buffer);
    let b_input = new flatbuffers.ByteBuffer(data);
    let b = new flatbuffers.Builder(1024);
    let mw = MessageWrapper.getRootAsMessageWrapper(b_input);
    console.log(`Received buffer length ${buffer.byteLength} and Type is ${mw.messageType()}`);
    switch (mw.messageType()) {
        case Message.RequestWifiAccesspoints:
            setTimeout(() => { sendResponseWifiAccesspoints(ws); }, 500);
            break;
        case Message.RequestSystemData:
            setTimeout(() => { sendResponseSystemData(ws); }, 500);
            break;
        case Message.RequestGetUserSettings:
            setTimeout(() => { sendResponseGetUserSettings(ws, <RequestGetUserSettings>mw.message(new RequestGetUserSettings())); }, 500);
            break;
        case Message.RequestSetUserSettings:
            setTimeout(() => { sendResponseSetUserSettings(ws, <RequestSetUserSettings>mw.message(new RequestSetUserSettings())); }, 100);
            break;
        case Message.RequestJournal:
            setTimeout(() => { sendResponseJournal(ws);}, 100);
            break;
        case Message.RequestTimeseries:
            setTimeout(()=>{sendResponseTimeseries(ws, <RequestTimeseries>mw.message(new RequestTimeseries())), 200});
        case Message.RequestWifiConnect:{
            setTimeout(()=>{sendResponseWifiConnectionSuccessOrFailed(ws, <RequestWifiConnect>mw.message(new RequestWifiConnect()));}, 3000);
        }
        default:
            break;
    }
}

let hostCert =fs.readFileSync("./../certificates/testserver.pem.crt").toString();
let hostPrivateKey = fs.readFileSync("./../certificates/testserver.pem.prvtkey").toString();

let server = https.createServer({key: hostPrivateKey, cert: hostCert}, (req, res) => {
    console.log(`Request received for '${req.url}'`);
    //var local_path = new URL(req.url).pathname;

    if (req.url == "/sensact") {
        const req_body_chunks: any[] = [];
        req.on("data", (chunk: any) => req_body_chunks.push(chunk));
        req.on("end", () => {
            //process(res, req.url!, Buffer.concat(req_body_chunks));
        });

    } else if (req.url == "/") {
        fs.readFile(BUNDLED_FILE, (err, data) => {
            res.writeHead(200, { 'Content-Type': 'text/html', "Content-Encoding": "br" });
            res.end(data);
        });
    } else {
        res.writeHead(404);
        res.end("Not found");
        return;
    }
});

server.on('upgrade', (req, sock, head) => {
    if (req.url == '/webmanager_ws') {
        wss.handleUpgrade(req, sock, head, ws => wss.emit('connection', ws, req));
    } else {
        sock.destroy();
    }
});

var messageChanger = 0;

wss.on('connection', (ws) => {
    ws.on('error', console.error);

    ws.on('message', (data: Buffer, isBinary: boolean) => {
        process(data, ws);
    });
});

server.on("error",(e)=>{
    console.error(e);
})

server.listen(PORT, () => {
    console.log(`Server is running on port ${PORT}`);
    const interval = setInterval(() => {
        let b = new flatbuffers.Builder(1024);
        let mw: number;
        switch (messageChanger) {
            case 0:
                let text = b.createString(`Dies ist eine Meldung ${new Date().getTime()}`);
                LiveLogItem.startLiveLogItem(b);
                LiveLogItem.addText(b, text);
                let li = LiveLogItem.endLiveLogItem(b);
                mw = MessageWrapper.createMessageWrapper(b, Message.LiveLogItem, li);
                b.finish(mw);
                //wss.clients.forEach(ws => ws.send(b.asUint8Array()));
                break;
            default:
                break;
        }
        messageChanger++;
        messageChanger %= 3;

    }, 1000);

});




