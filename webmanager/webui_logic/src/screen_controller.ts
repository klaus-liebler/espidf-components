import { Ip4 } from "./generated/webmanager";
import { MessageWrapper, } from "./generated/webmanager/message-wrapper";
import { AppManagement, WebsocketMessageListener } from "./app_management";
import { RECORD_SEPARATOR, UNIT_SEPARATOR, gel } from "./utils";

export enum ControllerState {
    CREATED,
    STARTED,
    PAUSED,
}

export abstract class ScreenController implements WebsocketMessageListener {
    constructor(protected appManagement: AppManagement) {

    }
    abstract onCreate(): void;
    abstract onFirstStart(): void;
    abstract onRestart(): void;
    abstract onPause(): void;
    abstract onMessage(messageWrapper:MessageWrapper):void;
}

export class ScreenControllerWrapper {
    constructor(public name: string, public state: ControllerState, public element: HTMLElement, public controller: ScreenController) { }
}

export class DefaultScreenController extends ScreenController {
    onMessage(messageWrapper: MessageWrapper): void {
        
    }
    onCreate(): void {

    }
    onFirstStart(): void {

    }
    onRestart(): void {

    }
    onPause(): void {

    }
}



export class WeblogScreenController extends ScreenController {
    onMessage(messageWrapper: MessageWrapper): void {
        
    }
    private tblLogs = <HTMLTableSectionElement>gel("tblLogs");

    private processLogEntries(txt: string) {
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

    onCreate(): void {
        //this.appManagement.registerWebsocketMessageTypes(this, MEssageType)
    }

    onFirstStart(): void {
        
    }
    onRestart(): void {
        
    }
    onPause(): void {
        
    }

}
