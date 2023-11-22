import { Mac6, RequestJournal, RequestRestart, RequestSystemData, ResponseSystemData } from "./flatbuffers_gen/webmanager";
import { Message } from "./flatbuffers_gen/webmanager/message";
import { MessageWrapper } from "./flatbuffers_gen/webmanager/message-wrapper";
import { findChipModel, findChipFeatures, findPartitionState, findPartitionSubtype } from "./esp32";
import { gel } from "./utils";
import { ScreenController } from "./screen_controller";
import * as flatbuffers from 'flatbuffers';
import { Severrity } from "./dialog_controller";
import { UPLOAD_URL } from "./constants";
import {LineChart, LineChartOptions} from './chartist/charts/LineChart/index';
import { AxisOptions } from "./chartist/core/types";


export class SystemScreenController extends ScreenController {
    private btnUpload = <HTMLInputElement>gel("btnUpload");
    private btnRestart = <HTMLInputElement>gel("btnRestart");
    private inpOtafile = <HTMLInputElement>gel("inpOtafile");
    private lblProgress = <HTMLInputElement>gel("lblProgress");
    private prgbProgress = <HTMLInputElement>gel("prgbProgress");
    private tblPartitions = <HTMLTableSectionElement>gel("tblPartitions");
    private tblParameters = <HTMLTableSectionElement>gel("tblParameters");

    private sendRequestRestart() {
        let b = new flatbuffers.Builder(1024);
        let n = RequestRestart.createRequestRestart(b);
        let mw = MessageWrapper.createMessageWrapper(b, Message.RequestRestart, n);
        b.finish(mw);
        this.appManagement.sendWebsocketMessage(b.asUint8Array());
    }

    private sendRequestSystemdata() {
        let b = new flatbuffers.Builder(1024);
        let n = RequestSystemData.createRequestSystemData(b);
        let mw = MessageWrapper.createMessageWrapper(b, Message.RequestSystemData, n);
        b.finish(mw);
        this.appManagement.sendWebsocketMessage(b.asUint8Array(), Message.ResponseSystemData, 3000);
    }

    onMessage(messageWrapper: MessageWrapper): void {
        if (messageWrapper.messageType() != Message.ResponseSystemData) {
            return;
        }
        let sd = <ResponseSystemData>messageWrapper.message(new ResponseSystemData());
        this.tblParameters.textContent = "";

        let secondsEpoch = sd.secondsEpoch();
        let localeDate = new Date(Number(1000n * secondsEpoch)).toLocaleString();
        this.insertParameter("Real Time Clock", localeDate + " [" + secondsEpoch.toString() + "secs]");
        this.insertParameter("Uptime [sec]", sd.secondsUptime().toString());
        this.insertParameter("Free Heap [byte]", sd.freeHeap());
        this.insertParameter("MAC Address WIFI_STA", this.mac6_2_string(sd.macAddressWifiSta()));
        this.insertParameter("MAC Address WIFI_SOFTAP", this.mac6_2_string(sd.macAddressWifiSoftap()));
        this.insertParameter("MAC Address BT", this.mac6_2_string(sd.macAddressBt()));
        this.insertParameter("MAC Address ETH", this.mac6_2_string(sd.macAddressEth()));
        this.insertParameter("MAC Address IEEE802154", this.mac6_2_string(sd.macAddressIeee802154()));
        this.insertParameter("Chip Model", findChipModel(sd.chipModel()));
        this.insertParameter("Chip Features", findChipFeatures(sd.chipFeatures()));
        this.insertParameter("Chip Revision", sd.chipRevision());
        this.insertParameter("Chip Cores", sd.chipCores());
        this.insertParameter("Chip Temperature", sd.chipTemperature().toLocaleString() + "°C");

        this.tblPartitions.textContent = "";
        for (let i = 0; i < sd.partitionsLength(); i++) {
            var row = this.tblPartitions.insertRow();
            row.insertCell().textContent = <string>sd.partitions(i)!.label();
            row.insertCell().textContent = findPartitionSubtype(sd.partitions(i)!.type(), sd.partitions(i)!.subtype());
            row.insertCell().textContent = sd.partitions(i)!.size().toString();
            row.insertCell().textContent = findPartitionState(sd.partitions(i)!.otaState());
            row.insertCell().textContent = sd.partitions(i)!.running().toString();
            row.insertCell().textContent = <string>sd.partitions(i)!.appName();
            row.insertCell().textContent = <string>sd.partitions(i)!.appVersion();
            row.insertCell().textContent = <string>sd.partitions(i)!.appDate();
            row.insertCell().textContent = <string>sd.partitions(i)!.appTime();
        }
    }

    private insertParameter(name: string, value: string | number) {
        var row = this.tblParameters.insertRow();
        row.insertCell().textContent = name;
        row.insertCell().textContent = value.toString();
    }

    private mac6_2_string(mac: Mac6 | null): string {
        if (!mac) return "No Mac";
        return `${mac.v(0)}:${mac.v(1)}:${mac.v(2)}:${mac.v(3)}:${mac.v(4)}:${mac.v(5)}`;
    }

    private startUpload(e: MouseEvent) {
        let otafiles = this.inpOtafile!.files!;
        if (otafiles.length == 0) {
            this.appManagement.DialogController().showOKDialog(0, "No file selected!");
            return;
        }

        this.inpOtafile.disabled = true;
        this.btnUpload.disabled = true;

        var file = otafiles[0];
        var xhr = new XMLHttpRequest();
        xhr.onreadystatechange = (e: Event) => {
            if (xhr.readyState == 4) {
                if (xhr.status == 200) {
                    document.open();
                    document.write(xhr.responseText);
                    document.close();
                } else if (xhr.status == 0) {
                    alert("Server closed the connection abruptly!");
                    location.reload();
                } else {
                    alert(xhr.status + " Error!\n" + xhr.responseText);
                    location.reload();
                }
            }
        };

        xhr.upload.onprogress = (e: ProgressEvent) => {

            let percent = (e.loaded / e.total * 100).toFixed(0);
            this.lblProgress.textContent = "Progress: " + percent + "%";
            this.prgbProgress.value = percent;
        };
        xhr.open("POST", UPLOAD_URL, true);
        xhr.send(file);

    }

    onCreate(): void {
        this.btnUpload.onclick = (e: MouseEvent) => this.startUpload(e);
        this.btnRestart.onclick = (e: MouseEvent) => {
            this.appManagement.DialogController().showOKCancelDialog(Severrity.WARN, "Are you rellay sure to restart the system", () => this.sendRequestRestart());


        };
        const ctx = <HTMLCanvasElement>document.getElementById('myChart')!;

        var data = {
            // A labels array that can contain any sort of values
            labels: ['Mon', 'Tue', 'Wed', 'Thu', 'Fri'],
            // Our series array that contains series objects or in this case series data arrays
            series: [
                [5, 2, 4, 2, 0]
            ]
        };



        let options: LineChartOptions<AxisOptions, AxisOptions>={};
    
        new LineChart(
            '#chartist_example',
            {
                labels: [1, 2, 3, 4, 5, 6, 7, 8],
                series: [[5, 9, 7, 8, 5, 3, 5, 4]]
            },
            {
                low: 0,
                showArea: true,
                width: '400px',
                height: '200px'
            });
        this.appManagement.registerWebsocketMessageTypes(this, Message.ResponseSystemData);

    }
    onFirstStart(): void {
        this.sendRequestSystemdata();
    }
    onRestart(): void {
        this.sendRequestSystemdata();
    }
    onPause(): void {
    }

}
