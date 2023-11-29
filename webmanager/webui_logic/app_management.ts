import { Message, MessageWrapper } from "./flatbuffers_gen/webmanager";
import { DialogController } from "./dialog_controller"

export interface WebsocketMessageListener{
    onMessage(messageWrapper:MessageWrapper):void;
}

export interface AppManagement
{
    DialogController():DialogController;
    MainElement():HTMLElement;
    registerWebsocketMessageTypes(listener: WebsocketMessageListener, ...messageType:number[]):void;
    sendWebsocketMessage(data:ArrayBuffer, messageToUnlock?:Array<Message>, maxWaitingTimeMs?:number):void;
};