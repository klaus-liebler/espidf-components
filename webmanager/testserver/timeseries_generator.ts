import { TimeGranularity } from "./flatbuffers_gen/webmanager";

function updateDate(date: Date, granularity: TimeGranularity) {
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

export function createTimeseries(granularity:TimeGranularity):ArrayBuffer
{
    let b = new ArrayBuffer(4096);
    let v=new DataView(b);
    let secondsEpoch=Math.floor(new Date().getTime()/1000);
    console.log(`Epoch Seconds orig=${secondsEpoch}`);
    let secondsEpochBI=BigInt(secondsEpoch);
    console.log(`Epoch Seconds BI=${secondsEpochBI}`);
    v.setBigInt64(0, secondsEpochBI);
    v.setBigInt64(8, BigInt(granularity));
    console.log(`Epoch Seconds get=${Number(v.getBigInt64(0))}`);

    var dataOffsetByte = 64;

    while (dataOffsetByte < 4096 / 2) {
        v.setInt16(dataOffsetByte, 20000*Math.sin(dataOffsetByte*0.002));
        dataOffsetByte += 2;
        v.setInt16(dataOffsetByte, 20000*Math.sin(dataOffsetByte*0.004));
        dataOffsetByte += 2;
        v.setInt16(dataOffsetByte, 20000*Math.sin(dataOffsetByte*0.008));
        dataOffsetByte += 2;
        v.setInt16(dataOffsetByte, 20000*Math.sin(dataOffsetByte*0.016));
        dataOffsetByte += 2;
    }
    return b;

}