import { UNIT_SEPARATOR, RECORD_SEPARATOR, GROUP_SEPARATOR, gel } from "./utils";

//const URL_PREFIX = "http://192.168.2.124";
const URL_PREFIX ="";
const UPLOAD_URL = URL_PREFIX + "/ota";
const INFO_URL = URL_PREFIX + "/systeminfo";
const MESSAGELOG_URL = URL_PREFIX + "/messagelog";
const RESTART_URL = URL_PREFIX + "/restart";

const chip2name = new Map<number, string>([[1, "ESP32"], [2, "ESP32-S2"], [9, "ESP32-S3"], [5, "ESP32-C3"], [6, "ESP32-H2"], [12, "ESP32-C2"]]);
const chipfeature = new Map<number, string>([[0, "Chip has embedded flash memory"], [1, "Chip has 2.4GHz WiFi"], [4, "Chip has Bluetooth LE"], [5, "Chip has Bluetooth Classic"], [6, "Chip has IEEE 802.15.4"], [7, "Chip has embedded psram"]]);/* Chip feature flags, used in esp_chip_info_t */

class PartitionInfo {
  private partitionstate = new Map<number, string>([
    [0, "ESP_OTA_IMG_NEW"],
    [1, "ESP_OTA_IMG_PENDING_VERIFY"],
    [2, "ESP_OTA_IMG_VALID"],
    [3, "ESP_OTA_IMG_INVALID"],
    [4, "ESP_OTA_IMG_ABORTED"],
    [0xFFFFFFFF, "ESP_OTA_IMG_UNDEFINED"]
  ]);
  private partitionsubtypesapp = new Map<number, string>([
    [0x00, "ESP_PARTITION_SUBTYPE_APP_FACTORY"],
    [0x10, "ESP_PARTITION_SUBTYPE_APP_OTA_0"],// = ESP_PARTITION_SUBTYPE_APP_OTA_MIN + 0,  //!< OTA partition 0
    [0x11, "ESP_PARTITION_SUBTYPE_APP_OTA_1"],// = ESP_PARTITION_SUBTYPE_APP_OTA_MIN + 1,  //!< OTA partition 1
    [0x12, "ESP_PARTITION_SUBTYPE_APP_OTA_2"],// = ESP_PARTITION_SUBTYPE_APP_OTA_MIN + 2,  //!< OTA partition 2
    [0x13, "ESP_PARTITION_SUBTYPE_APP_OTA_3"],// = ESP_PARTITION_SUBTYPE_APP_OTA_MIN + 3,  //!< OTA partition 3
    [0x14, "ESP_PARTITION_SUBTYPE_APP_OTA_4"],// = ESP_PARTITION_SUBTYPE_APP_OTA_MIN + 4,  //!< OTA partition 4
    [0x15, "ESP_PARTITION_SUBTYPE_APP_OTA_5"],// = ESP_PARTITION_SUBTYPE_APP_OTA_MIN + 5,  //!< OTA partition 5
    [0x16, "ESP_PARTITION_SUBTYPE_APP_OTA_6"],// = ESP_PARTITION_SUBTYPE_APP_OTA_MIN + 6,  //!< OTA partition 6
    [0x17, "ESP_PARTITION_SUBTYPE_APP_OTA_7"],// = ESP_PARTITION_SUBTYPE_APP_OTA_MIN + 7,  //!< OTA partition 7
    [0x18, "ESP_PARTITION_SUBTYPE_APP_OTA_8"],// = ESP_PARTITION_SUBTYPE_APP_OTA_MIN + 8,  //!< OTA partition 8
    [0x19, "ESP_PARTITION_SUBTYPE_APP_OTA_9"],// = ESP_PARTITION_SUBTYPE_APP_OTA_MIN + 9,  //!< OTA partition 9
    [0x1A, "ESP_PARTITION_SUBTYPE_APP_OTA_10"],// = ESP_PARTITION_SUBTYPE_APP_OTA_MIN + 10,//!< OTA partition 10
    [0x1B, "ESP_PARTITION_SUBTYPE_APP_OTA_11"],// = ESP_PARTITION_SUBTYPE_APP_OTA_MIN + 11,//!< OTA partition 11
    [0x1C, "ESP_PARTITION_SUBTYPE_APP_OTA_12"],// = ESP_PARTITION_SUBTYPE_APP_OTA_MIN + 12,//!< OTA partition 12
    [0x1D, "ESP_PARTITION_SUBTYPE_APP_OTA_13"],// = ESP_PARTITION_SUBTYPE_APP_OTA_MIN + 13,//!< OTA partition 13
    [0x1E, "ESP_PARTITION_SUBTYPE_APP_OTA_14"],// = ESP_PARTITION_SUBTYPE_APP_OTA_MIN + 14,//!< OTA partition 14
    [0x1F, "ESP_PARTITION_SUBTYPE_APP_OTA_15"],// = ESP_PARTITION_SUBTYPE_APP_OTA_MIN + 15,//!< OTA partition 15
    [0x20, "ESP_PARTITION_SUBTYPE_APP_TEST"],
  ]);
  private partitionsubtypesdata = new Map<number, string>([
    [0x00, "ESP_PARTITION_SUBTYPE_DATA_OTA"],// = 0x00,                                    //!< OTA selection partition
    [0x01, "ESP_PARTITION_SUBTYPE_DATA_PHY"],// = 0x01,                                    //!< PHY init data partition
    [0x02, "ESP_PARTITION_SUBTYPE_DATA_NVS"],// = 0x02,                                    //!< NVS partition
    [0x03, "ESP_PARTITION_SUBTYPE_DATA_COREDUMP"],// = 0x03,                               //!< COREDUMP partition
    [0x04, "ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS"],// = 0x04,                               //!< Partition for NVS keys
    [0x05, "ESP_PARTITION_SUBTYPE_DATA_EFUSE_EM"],// = 0x05,                               //!< Partition for emulate eFuse bits
    [0x06, "ESP_PARTITION_SUBTYPE_DATA_UNDEFINED"],// = 0x06,                              //!< Undefined (or unspecified) data partition

    [0x80, "ESP_PARTITION_SUBTYPE_DATA_ESPHTTPD"],// = 0x80,                               //!< ESPHTTPD partition
    [0x81, "ESP_PARTITION_SUBTYPE_DATA_FAT"],// = 0x81,                                    //!< FAT partition
    [0x82, "ESP_PARTITION_SUBTYPE_DATA_SPIFFS"],// = 0x82,                         
  ]);
  constructor(public label: string, public type: number, public subtype: number, public size: number, public ota_state: number, public running: boolean, public app_name: string, public app_version: string, public app_date: string, public app_time: string) { }

  public static ParseFromString(str: string): PartitionInfo {
    let items = str.split(UNIT_SEPARATOR);
    return new PartitionInfo(items[0], parseInt(items[1]), parseInt(items[2]), parseInt(items[3]), parseInt(items[4]), parseInt(items[5]) == 1, items[6], items[7], items[8], items[9]);
  }

  public findPartitionState(): string {
    return this.partitionstate.has(this.ota_state) ? this.partitionstate.get(this.ota_state) : "Unknown State " + this.ota_state.toFixed(0);
  }

  public findPartitionSubtype(): string {
    switch (this.type) {
      case 0x00:
        return this.partitionsubtypesapp.has(this.subtype) ? this.partitionsubtypesapp.get(this.subtype) : "Unknown App Subtype";
      case 0x01:
        return this.partitionsubtypesdata.has(this.subtype) ? this.partitionsubtypesdata.get(this.subtype) : "Unknown Data Subtype";
      default:
        return "Unknown Partition Type";
    }
  }
}

class AppController {

  private btnUpload = <HTMLInputElement>gel("btnUpload");
  private btnRestart = <HTMLInputElement>gel("btnRestart");
  private inpOtafile = <HTMLInputElement>gel("inpOtafile");
  private lblProgress = <HTMLInputElement>gel("lblProgress");
  private prgbProgress = <HTMLInputElement>gel("prgbProgress");
  private tblPartitions = <HTMLTableSectionElement>gel("tblPartitions");
  private tblParameters = <HTMLTableSectionElement>gel("tblParameters");
  private tblLogs = <HTMLTableSectionElement>gel("tblLogs");
  constructor() { }

  private findChipName(chipIdString: string): string {
    let chipId = parseInt(chipIdString);
    return chip2name.has(chipId) ? chip2name.get(chipId) : "Unknown Chip";
  }

  private findFeatures(featuresBitSetString: string): string {
    let featuresBitSet = parseInt(featuresBitSetString);
    let s = "";
    for (let entry of chipfeature.entries()) {
      let mask = 1 << entry[0];
      if ((featuresBitSet & mask) != 0) {
        s += entry[1] + ", ";
      }
    }
    return s;
  }

  private startUpload(e: MouseEvent) {
    let otafiles = this.inpOtafile.files;
    if (otafiles.length == 0) {
      alert("No file selected!");
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
          location.reload()
        } else {
          alert(xhr.status + " Error!\n" + xhr.responseText);
          location.reload()
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

  public startup() {
    this.btnUpload.onclick = (e: MouseEvent) => this.startUpload(e);
    this.btnRestart.onclick = (e:MouseEvent)=>{
      fetch(RESTART_URL, {method: "GET", headers: {"Content-Type": "text/plain"}}).catch((e) => console.log(e));
    };

    fetch(INFO_URL, {
      method: "GET",
      headers: {
        "Content-Type": "text/plain",
      },
    })
      .then((response) => response.text()
      )
      .then((txt) => {
        if (txt.length == 0) {
          console.error("Received an empty string from server");
          return;
        }
        let groups = txt.split(GROUP_SEPARATOR).filter(r => r !== "");
        if (groups.length != 2) {
          console.error("groups.length!=2");
          return;
        }
        this.processPartitions(groups[0]);
        this.processParameters(groups[1]);
      }
      )
      .catch((e) => console.log(e));

      fetch(MESSAGELOG_URL, {
        method: "GET",
        headers: {
          "Content-Type": "text/plain",
        },
      })
        .then((response) => response.text()
        )
        .then((txt) => {
          if (txt.length == 0) {
            console.error("Received an empty string from server");
            return;
          }
          let groups = txt.split(GROUP_SEPARATOR).filter(r => r !== "");
          if (groups.length != 1) {
            console.error("groups.length!=1");
            return;
          }
          this.processLogEntries(groups[0]);
        }
        )
        .catch((e) => console.log(e));

  }
  processLogEntries(txt: string) {
    let records = txt.split(RECORD_SEPARATOR).filter(r => r !== "");
    this.tblLogs.textContent = "";

    for (const str of records) {
      let items = str.split(UNIT_SEPARATOR).filter(r => r !== "");
      var row = this.tblLogs.insertRow();
      let secondsEpoch = parseInt(items[0]);
      let localeDate = new Date(1000 * secondsEpoch).toLocaleString();
      row.insertCell().textContent = localeDate;
      row.insertCell().textContent = items[1];
      row.insertCell().textContent = items[2];
      row.insertCell().textContent = items[3];
      row.insertCell().textContent = items[4];
    }
  }
  private insertParameter(name: string, value: string) {
    var row = this.tblParameters.insertRow();
    row.insertCell().textContent = name;
    row.insertCell().textContent = value;
  }

  private processParameters(txt: string) {
    let records = txt.split(RECORD_SEPARATOR).filter(r => r !== "");
    if (records.length < 12) {
      console.error("records.length<12");
      return;
    }
    let secondsEpoch = parseInt(records[0]);
    let localeDate = new Date(1000 * secondsEpoch).toLocaleString();
    this.insertParameter("Real Time Clock", localeDate + " [" + secondsEpoch.toFixed() + "secs]");
    this.insertParameter("Uptime [sec]", records[1]);
    this.insertParameter("Free Heap [byte]", records[2]);
    this.insertParameter("MAC Address WIFI_STA", records[3]);
    this.insertParameter("MAC Address WIFI_SOFTAP", records[4]);
    this.insertParameter("MAC Address BT", records[5]);
    this.insertParameter("MAC Address ETH", records[6]);
    this.insertParameter("MAC Address IEEE802154", records[7]);
    this.insertParameter("Chip Model", this.findChipName(records[8]));
    this.insertParameter("Chip Features", this.findFeatures(records[9]));
    this.insertParameter("Chip Revision", records[10]);
    this.insertParameter("Chip Cores", records[11]);
  }

  private processPartitions(txt: string) {
    let records = txt.split(RECORD_SEPARATOR).filter(r => r !== "");
    this.tblPartitions.textContent = "";

    for (const str of records) {
      let partinfo: PartitionInfo = PartitionInfo.ParseFromString(str);
      var row = this.tblPartitions.insertRow();
      row.insertCell().textContent = partinfo.label;
      row.insertCell().textContent = partinfo.findPartitionSubtype();
      row.insertCell().textContent = partinfo.size.toString();
      row.insertCell().textContent = partinfo.findPartitionState();
      row.insertCell().textContent = partinfo.running.toString();
      row.insertCell().textContent = partinfo.app_name;
      row.insertCell().textContent = partinfo.app_version;
      row.insertCell().textContent = partinfo.app_date;
      row.insertCell().textContent = partinfo.app_time;
    }

  }
}

let app: AppController;
document.addEventListener("DOMContentLoaded", (e) => {
  app = new AppController();
  app.startup();
});