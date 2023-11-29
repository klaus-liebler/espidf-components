import { Severrity } from "./dialog_controller";
import {Message, MessageWrapper, RequestWifiAccesspoints, RequestWifiConnect, RequestWifiDisconnect, ResponseWifiAccesspoints, ResponseWifiConnectSuccessful, UpdateReasonCode } from "./flatbuffers_gen/webmanager";
import { ScreenController } from "./screen_controller";
import { gel, $, gqsa, Html } from "./utils";
import * as flatbuffers from 'flatbuffers';

const icon_lock_template = document.getElementById("icon-lock") as HTMLTemplateElement;
const icon_rssi_template = document.getElementById("icon-wifi") as HTMLTemplateElement;


export class WifimanagerController extends ScreenController {
    onConnectToWifi(ssid: string): void {
        this.appManagement.DialogController().showEnterPasswordDialog(Severrity.INFO, "Enter Password for Wifi", (password: string) => {
            this.sendRequestWifiConnect(ssid, password);
        });
    }

    private sendRequestWifiAccesspoints(forceNewSearch:boolean) {
        let b = new flatbuffers.Builder(1024);
        let n = RequestWifiAccesspoints.createRequestWifiAccesspoints(b, forceNewSearch);
        let mw = MessageWrapper.createMessageWrapper(b, Message.RequestWifiAccesspoints, n);
        b.finish(mw);
        this.appManagement.sendWebsocketMessage(b.asUint8Array(), [Message.ResponseWifiAccesspoints], 30000);
    }

    private sendRequestWifiConnect(ssid: string, password: string) {
        let b = new flatbuffers.Builder(1024);
        let ssidOffset = b.createString(ssid);
        let passwordOffset = b.createString(password);
        let n = RequestWifiConnect.createRequestWifiConnect(b, ssidOffset, passwordOffset);
        let mw = MessageWrapper.createMessageWrapper(b, Message.RequestWifiConnect, n);
        b.finish(mw);
        this.appManagement.sendWebsocketMessage(b.asUint8Array(), [Message.ResponseWifiConnectSuccessful, Message.ResponseWifiConnectFailed], 30000);
    }

    private sendRequestWifiDisconnect() {
        let b = new flatbuffers.Builder(1024);
        let n = RequestWifiDisconnect.createRequestWifiDisconnect(b)
        let mw = MessageWrapper.createMessageWrapper(b, Message.RequestWifiDisconnect, n);
        b.finish(mw);
        this.appManagement.sendWebsocketMessage(b.asUint8Array());
    }

    onCreate(): void {
        this.appManagement.registerWebsocketMessageTypes(this, Message.ResponseWifiAccesspoints);
        this.appManagement.registerWebsocketMessageTypes(this, Message.ResponseWifiConnectFailed);
        this.appManagement.registerWebsocketMessageTypes(this, Message.ResponseWifiConnectSuccessful);
        this.appManagement.registerWebsocketMessageTypes(this, Message.ResponseWifiDisconnect);

        gel("btnWifiShowDetails").onclick = () => this.appManagement.DialogController().showOKDialog(Severrity.INFO, "Das sind die Details", (s) => { });
        gel("btnWifiUpdateList").onclick = () =>{this.sendRequestWifiAccesspoints(false);};
        gel("btnWifiDisconnect").onclick=()=> this.appManagement.DialogController().showOKCancelDialog(Severrity.WARN, "Möchten Sie wirklich die bestehende Verbindung trennen und damit auch vom ESP32 löschen?", (ok) => {if(ok) this.sendRequestWifiDisconnect();});
    }

    private ip4_2_string(ip: number): string {
        return `${(ip>>24)&0xFF}.${(ip>>16)&0xFF}.${(ip>>8)&0xFF}.${(ip>>0)&0xFF}`;
    }

    onFirstStart(): void {
        this.sendRequestWifiAccesspoints(false);
    }
    onRestart(): void {

    }
    onPause(): void {

    }

    onResponseWifiAccesspoints(r: ResponseWifiAccesspoints) {
        let ssid2index = new Map<string, number>();
        for (let i = 0; i < r.accesspointsLength(); i++) {
            let key = r.accesspoints(i)!.ssid() + "_" + r.accesspoints(i)!.authMode();
            let ap_exist = ssid2index.get(key);
            if (ap_exist === undefined) {
                ssid2index.set(key, i);
            }
        }

        let access_points_list = [...ssid2index.values()];
        access_points_list.sort((a, b) => {
            //sort according to rssi
            var x = r.accesspoints(a)!.rssi();
            var y = r.accesspoints(b)!.rssi();
            return x < y ? 1 : x > y ? -1 : 0;
        });


        let table = gel("tblAccessPointList");
        table.textContent = "";
        for (let i of access_points_list) {
            let tr = Html(table, "tr");
            let td_rssi = Html(tr, "td");
            let figure_rssi = document.importNode(icon_rssi_template.content, true);
            td_rssi.appendChild(figure_rssi);
            let td_auth = Html(tr, "td");
            if (r.accesspoints(i)!.authMode() != 0) {
                td_auth.appendChild(document.importNode(icon_lock_template.content, true));
            }
            let rssiIcon = td_rssi.children[0];
            (rssiIcon.children[0] as SVGPathElement).style.fill = r.accesspoints(i)!.rssi() >= -60 ? "black" : "LightGray";
            (rssiIcon.children[1] as SVGPathElement).style.fill = r.accesspoints(i)!.rssi() >= -67 ? "black" : "LightGray";
            (rssiIcon.children[2] as SVGPathElement).style.fill = r.accesspoints(i)!.rssi() >= -75 ? "black" : "LightGray";
            Html(tr, "td", [], [], `${r.accesspoints(i)!.ssid()} [${r.accesspoints(i)!.rssi()}dB]`);
            let buttonParent = Html(tr, "td");
            let button = <HTMLInputElement>Html(buttonParent, "input", ["type", "button", "value", `Connect to ${r.accesspoints(i)!.ssid()}`]);
            button.onclick = () => {
                this.onConnectToWifi(r.accesspoints(i)!.ssid()!);
            };
        }
    }

    onResponseWifiConnectSuccessful(r: ResponseWifiConnectSuccessful) {
        console.info("Got connection!");
        this.appManagement.DialogController().showOKDialog(Severrity.SUCCESS, `Connection to ${r.ssid()} was successful. `, () => { });
        gqsa(".current_ssid", (e) => e.textContent = r.ssid());
        gqsa(".current_ip", (e) => e.textContent = this.ip4_2_string(r.ip()));
        gqsa(".current_netmask", (e) => e.textContent = this.ip4_2_string(r.netmask()));
        gqsa(".current_gw", (e) => e.textContent = this.ip4_2_string(r.gateway()));
        gqsa(".current_hostname", (e) => e.textContent = r.hostname());
        gqsa(".current_rssi", (e) => e.textContent = r.rssi().toLocaleString()+"dB");
    }

    onMessage(messageWrapper: MessageWrapper): void {
        switch (messageWrapper.messageType()) {
            case Message.ResponseWifiAccesspoints: 
                this.onResponseWifiAccesspoints(<ResponseWifiAccesspoints>messageWrapper.message(new ResponseWifiAccesspoints()));
                break;
            case Message.ResponseWifiConnectSuccessful:
                this.onResponseWifiConnectSuccessful(<ResponseWifiConnectSuccessful>messageWrapper.message(new ResponseWifiConnectSuccessful()));
               
                break;
            case Message.ResponseWifiDisconnect:
                console.log("Manual disconnect requested...");
                this.appManagement.DialogController().showOKDialog(Severrity.INFO, "Manual disconnection was successful. ", () => { });
                break;
            case Message.ResponseWifiConnectFailed:
                console.info("Connection attempt failed!");
                this.appManagement.DialogController().showOKDialog(Severrity.ERROR, "Connection attempt failed! ", () => { });
            break;
        }



    }


}