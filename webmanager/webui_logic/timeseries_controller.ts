import { RequestGetUserSettings, RequestSetUserSettings, RequestTimeseries, ResponseGetUserSettings, ResponseSetUserSettings, SettingWrapper, TimeGranularity } from "./flatbuffers_gen/webmanager";
import { Message } from "./flatbuffers_gen/webmanager/message";
import { MessageWrapper } from "./flatbuffers_gen/webmanager/message-wrapper";
import { ScreenController } from "./screen_controller";
import * as flatbuffers from 'flatbuffers';
import { LineChart, LineChartOptions } from './chartist/charts/LineChart/index';
import { AxisOptions } from "./chartist/core/types";


export class TimeseriesController extends ScreenController {
    private chartSeconds?:LineChart;
    onMessage(messageWrapper: MessageWrapper): void {
        throw new Error("TimeseriesController does not expect 'normal' messages");
    }


    private sendRequestTimeseries() {
        let b = new flatbuffers.Builder(256);
        let n = RequestTimeseries.createRequestTimeseries(b, 0, 0);
        let mw = MessageWrapper.createMessageWrapper(b, Message.RequestTimeseries, n);
        b.finish(mw);
        this.appManagement.sendWebsocketMessage(b.asUint8Array(), [Message.ResponseTimeseriesDummy], 3000);
    }

    private updateDate(date: Date, granularity: TimeGranularity) {
        switch (granularity) {
            case TimeGranularity.TEN_SECONDS:
                date.setSeconds(date.getSeconds() + 10);
                break;
            case TimeGranularity.ONE_MINUTE:
                date.setMinutes(date.getMinutes() + 1);
                break;
            case TimeGranularity.ONE_HOUR:
                date.setHours(date.getHours() + 1);
                break;
            case TimeGranularity.ONE_DAY:
                date.setDate(date.getDate() + 1); //Welche Chancen/Grenzen sehen Sie für den Transfer in ihr eigenes Fach?
                break;
            default:
                break;
        }
    }



    onTimeseriesMessage(data: ArrayBuffer): void {
        var dataView = new DataView(data);
        var epochSeconds = dataView.getBigInt64(0);
        var granularity:TimeGranularity = (Number(dataView.getBigInt64(8)));
        console.log(`Start epochSeconds= ${epochSeconds} Granularity=${granularity}`);
        var date = new Date(Number(epochSeconds) * 1000);
        var labels: Array<string> = [];
        var series: Array<Array<number>> = [[], [], [], []];

        var dataOffsetByte = 64;
        while (dataOffsetByte < 4096 / 2) {
            if((dataOffsetByte-64)%256==0){
                labels.push(date.toLocaleString());
            }
            else{
                labels.push("");
            }
            series[0].push(dataView.getInt16(dataOffsetByte));
            dataOffsetByte += 2;
            series[1].push(dataView.getInt16(dataOffsetByte));
            dataOffsetByte += 2;
            series[2].push(dataView.getInt16(dataOffsetByte));
            dataOffsetByte += 2;
            series[3].push(dataView.getInt16(dataOffsetByte));
            dataOffsetByte += 2;
            this.updateDate(date, granularity);
        }

        if(granularity==TimeGranularity.TEN_SECONDS){
            this.chartSeconds!.update({labels, series});
        }
    }



    onCreate(): void {
        let options: LineChartOptions<AxisOptions, AxisOptions> = {
            axisX:{
                scaleMinSpace:40
            },
            showArea:false,
            width: '100%',
            height:"200px",

        };

        this.chartSeconds=new LineChart(
            '#timeseries_seconds',
            {
                labels: [1, 2, 3, 4, 5, 6, 7, 8],
                series: [[5, 9, 7, 8, 5, 3, 5, 4]]
            },
            options);
    }

    onFirstStart(): void {
        this.sendRequestTimeseries();
    }
    onRestart(): void {
        this.sendRequestTimeseries();
    }
    onPause(): void {
    }

}
