import * as http from "node:http";
import * as fs from "node:fs";
import * as flatbuffers from 'flatbuffers';
import { WebSocketServer, WebSocket, RawData } from "ws";
import { MessageWrapper } from "./generated/webmanager/message-wrapper";
import { Message } from "./generated/webmanager/message";
import { ResponseSystemData } from "./generated/webmanager/response-system-data";
import { PartitionInfo } from "./generated/webmanager/partition-info";
import { Mac6, } from "./generated/webmanager/mac6";
import { LiveLogItem } from "./generated/webmanager/live-log-item";
import { ResponseWifiAccesspoints } from "./generated/webmanager/response-wifi-accesspoints";
import { AccessPoint } from "./generated/webmanager/access-point";


const PORT = 3000;
const BUNDLED_FILE = "../dist_webui/bundeled/app.html";

const wss = new WebSocketServer({ noServer: true });

function sendResponseSystemData(ws: WebSocket){
    let b = new flatbuffers.Builder(1024);
    let labelOffset=b.createString("Label");
    let appnameOffset=b.createString("AppName");
    let appversionOffset=b.createString("AppVersion");
    let appdateOffset=b.createString("AppDate");
    let apptimeOffset=b.createString("AppTime");
    let piOffset=PartitionInfo.createPartitionInfo(b, labelOffset, 1,1,1,1,true,appnameOffset, appversionOffset, appdateOffset, apptimeOffset);

    let piVecOffset = ResponseSystemData.createPartitionsVector(b, [piOffset]);
    ResponseSystemData.startResponseSystemData(b);
    ResponseSystemData.addChipCores(b, 2);
    ResponseSystemData.addChipFeatures(b, 255);
    ResponseSystemData.addPartitions(b, piVecOffset);
    ResponseSystemData.addChipModel(b, 2);
    ResponseSystemData.addChipRevision(b,3);
    ResponseSystemData.addChipTemperature(b, 23.4);
    ResponseSystemData.addFreeHeap(b, 1203);
    ResponseSystemData.addMacAddressBt(b, Mac6.createMac6(b, [1,2,3,4,5,6]));
    ResponseSystemData.addMacAddressEth(b, Mac6.createMac6(b, [1,2,3,4,5,6]));
    ResponseSystemData.addMacAddressIeee802154(b, Mac6.createMac6(b, [1,2,3,4,5,6]));
    ResponseSystemData.addMacAddressWifiSoftap(b, Mac6.createMac6(b, [1,2,3,4,5,6]));
    ResponseSystemData.addMacAddressWifiSta(b, Mac6.createMac6(b, [1,2,3,4,5,6]));
    ResponseSystemData.addSecondsEpoch(b, new Date().getTime()/1000);
    ResponseSystemData.addSecondsUptime(b, 10);
    let rsd=ResponseSystemData.endResponseSystemData(b);
    b.finish(MessageWrapper.createMessageWrapper(b, Message.ResponseSystemData, rsd));
    ws.send(b.asUint8Array());
}

function sendResponseWifiAccesspoints(ws: WebSocket){
    let b = new flatbuffers.Builder(1024);
    let mw:number;
    let ap0Offset=AccessPoint.createAccessPoint(b,b.createString("MySSID0"), 11, -72, 2);
    let ap1Offset=AccessPoint.createAccessPoint(b,b.createString("MySSID1"), 11, -62, 2);
    let accesspointsOffset = ResponseWifiAccesspoints.createAccesspointsVector(b, [ap0Offset, ap1Offset]);
    let r=ResponseWifiAccesspoints.createResponseWifiAccesspoints(b, b.createString("MyHostnameKL"), b.createString("MySsidApKL"), accesspointsOffset);
    b.finish(MessageWrapper.createMessageWrapper(b, Message.ResponseWifiAccesspoints, r));
    ws.send(b.asUint8Array());
}


function process(buffer: Buffer, ws: WebSocket) {
    let data = new Uint8Array(buffer);
    let b_input = new flatbuffers.ByteBuffer(data);
    let b = new flatbuffers.Builder(1024);
    let mw=MessageWrapper.getRootAsMessageWrapper(b_input);
    console.log(`Received buffer length ${buffer.byteLength} and Type is ${mw.messageType()}`);
    switch (mw.messageType()) {
        case Message.RequestWifiAccesspoints:
            setTimeout(()=>{sendResponseWifiAccesspoints(ws);}, 500);
            break;
        case Message.RequestSystemData:
            setTimeout(()=>{sendResponseSystemData(ws);}, 500);
        default:
            break;
    }
}

let server = http.createServer((req, res) => {
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
            res.writeHead(200, { 'Content-Type': 'text/html' });
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

var messageChanger=0;

wss.on('connection', (ws) => {
    ws.on('error', console.error);

    ws.on('message', (data: Buffer, isBinary: boolean) => {
        process(data, ws);
    });
});

server.listen(3000, () => {
    console.log(`Server is running on port ${PORT}`);
    const interval = setInterval(()=>{
        let b = new flatbuffers.Builder(1024);
        let mw:number;
        switch (messageChanger) {
            case 0:
                let text = b.createString(`Dies ist eine Meldung ${new Date().getTime()}`);
                LiveLogItem.startLiveLogItem(b);
                LiveLogItem.addText(b, text);
                let li = LiveLogItem.endLiveLogItem(b);
                mw = MessageWrapper.createMessageWrapper(b, Message.LiveLogItem, li);
                b.finish(mw);
                wss.clients.forEach(ws=>ws.send(b.asUint8Array()));
                break;       
            default:
                break;
        }
        messageChanger++;
        messageChanger%=3;

      }, 1000);
      
});