import { MessageWrapper, } from "./flatbuffers_gen/webmanager/message-wrapper";
import { AppManagement, WebsocketMessageListener } from "./app_management";
import { gel, $ } from "./utils";
import { DialogController } from "./dialog_controller";
import { Message, RequestJournal, ResponseJournal } from "./flatbuffers_gen/webmanager";
import * as flatbuffers from 'flatbuffers';

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

export class ScreenControllerWrapper  implements AppManagement{

    public controller!: ScreenController; 

    constructor(public name: string, public state: ControllerState, public element: HTMLElement, private parent:AppManagement) { }
    DialogController(): DialogController {
        return this.parent.DialogController();
    }
    MainElement(): HTMLElement {
        return this.element;
    }
    registerWebsocketMessageTypes(listener: WebsocketMessageListener, ...messageType: number[]): void {
        return this.parent.registerWebsocketMessageTypes(listener, ...messageType);
    }
    sendWebsocketMessage(data: ArrayBuffer, messageToUnlock?: Message | undefined, maxWaitingTimeMs?: number | undefined): void {
        return this.parent.sendWebsocketMessage(data, messageToUnlock, maxWaitingTimeMs);
    }
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


const dateTimeOptions:Intl.DateTimeFormatOptions={
    year:"numeric",
    month:"2-digit",
    day:"2-digit",
    hour:"2-digit",
    minute:"2-digit",
    second:"2-digit",
}
export class WeblogScreenController extends ScreenController {
    onMessage(messageWrapper: MessageWrapper): void {
        let res = <ResponseJournal>messageWrapper.message(new ResponseJournal());
        this.tblLogs.innerText="";
        for (let i = 0; i < res.journalItemsLength(); i++) {
            let item = res.journalItems(i);
            if(!item)return;
            var row = this.tblLogs.insertRow();
            let secondsEpoch = item.lastMessageTimestamp()!;
            let localeDate = new Date(1000*Number(secondsEpoch)).toLocaleString("de-DE", dateTimeOptions);
            row.insertCell().textContent = localeDate;
            row.insertCell().textContent = item.messageString();
            row.insertCell().textContent = item.messageString();
            row.insertCell().textContent = item.messageData().toString();
            row.insertCell().textContent = item.messageCount().toString();
        }
    }
    private tblLogs = <HTMLTableSectionElement>$("#tblLogs");

    sendRequestJournal(){
        let b = new flatbuffers.Builder(256);
        let n = RequestJournal.createRequestJournal(b);
        let mw = MessageWrapper.createMessageWrapper(b, Message.RequestJournal, n);
        b.finish(mw);
        this.appManagement.sendWebsocketMessage(b.asUint8Array(), Message.ResponseJournal, 3000);
    }

    onCreate(): void {
        this.appManagement.registerWebsocketMessageTypes(this, Message.ResponseJournal)
        setTimeout(()=>this.sendRequestJournal(),1000);
        $("#btnUpdateJournal").onclick=()=>{
            this.sendRequestJournal();
        }
    }

    onFirstStart(): void {
        
    }
    onRestart(): void {
        
    }
    onPause(): void {
        
    }

}
