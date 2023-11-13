import { Message, MessageWrapper } from "./generated/webmanager";
import { DialogController } from "./dialog_controller"

export interface WebsocketMessageListener{
    onMessage(messageWrapper:MessageWrapper):void;
}

export interface AppManagement
{
    DialogController():DialogController;
    registerWebsocketMessageTypes(listener: WebsocketMessageListener, ...messageType:number[]):void;
    sendWebsocketMessage(data:ArrayBuffer, messageToUnlock?:Message, maxWaitingTimeMs?:number):void;
};