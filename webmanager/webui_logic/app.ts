import { LiveLogItem, Message } from "./flatbuffers_gen/webmanager";
import { MessageWrapper} from "./flatbuffers_gen/webmanager/message-wrapper";
import { AppManagement, WebsocketMessageListener } from "./app_management";
import { DialogController, Severrity } from "./dialog_controller";
import { ControllerState, ScreenController, ScreenControllerWrapper, WeblogScreenController } from "./screen_controller";
import { SystemScreenController } from "./systemscreencontroller";
import * as flatbuffers from 'flatbuffers';
import { gel } from "./utils";
import { WifimanagerController } from "./wifimanager_controller";
import { WS_URL } from "./constants";
import { UsersettingsController } from "./usersettings_controller";

const ANSI_ESCAPE = new RegExp("(\\x9B|\\x1B\\[)[0-?]*[ -\\/]*[@-~]");
const MAX_MESSAGE_COUNT = 20;

class AppController implements AppManagement, WebsocketMessageListener {
  
  private scroller = <HTMLDivElement>gel('scroller');
  private anchor = <HTMLDivElement>gel('anchor');
  private modal = <HTMLDivElement>gel("modal");
  private socket: WebSocket;
  private messageCount = 0;
  

  private screenControllers: Map<string, ScreenControllerWrapper>;
  private dialogController: DialogController;
  private messageType2listener: Map<number, Array<WebsocketMessageListener>>;
  private messageToUnlock:Message=Message.NONE;
  private modalSpinnerInterval:number=0;

  public DialogController() { return this.dialogController; };

  private activateScreen(screenNameToActivate:string){
    this.screenControllers.forEach((wrapper, name) => {
      if (name == screenNameToActivate) {
        wrapper.element.style.display = "block";
        if (wrapper.state == ControllerState.CREATED) {
          wrapper.controller.onFirstStart();
          wrapper.state = ControllerState.STARTED;
        }
        else {
          wrapper.controller.onRestart();
          wrapper.state = ControllerState.STARTED;
        }
      } else {
        wrapper.element.style.display = "none";
        if (wrapper.state == ControllerState.STARTED) {
          wrapper.controller.onPause();
          wrapper.state = ControllerState.PAUSED;
        }
      }
    });
  }

  public AddScreenController<T extends ScreenController>(nameInNavAndInMain: string, type: { new(m:AppManagement): T ;}) {
    var mainElement = <HTMLElement>document.querySelector(`main[data-nav="${nameInNavAndInMain}"]`);
    var anchorElement = <HTMLAnchorElement>document.querySelector(`a[data-nav="${nameInNavAndInMain}"]`);
    var w = new ScreenControllerWrapper(nameInNavAndInMain, ControllerState.CREATED, mainElement, this);
    let controllerObject=new type(w);
    w.controller=controllerObject;
    this.screenControllers.set(nameInNavAndInMain, w);
    anchorElement.onclick = (e: MouseEvent) => {e.preventDefault();this.activateScreen(nameInNavAndInMain);};
    controllerObject.onCreate();
  }

  constructor() {
    this.screenControllers = new Map<string, ScreenControllerWrapper>();
    this.messageType2listener = new Map<number, [WebsocketMessageListener]>;
    this.dialogController = new DialogController(this);
    this.socket = new WebSocket(WS_URL);
    this.registerWebsocketMessageTypes(this, Message.LiveLogItem);
  }
  MainElement(): HTMLElement {
    throw new Error("May not be called");
  }
  private modalSpinnerTimeout(){
    this.setModal(false);
    this.dialogController.showOKDialog(Severrity.ERROR, "Server did not respond");
  }

  sendWebsocketMessage(data: ArrayBuffer, messageToUnlock:Message=Message.NONE, maxWaitingTimeMs:number=2000): void {
    this.messageToUnlock=messageToUnlock;
    if(messageToUnlock!=Message.NONE){
      this.setModal(true);
      this.modalSpinnerInterval=setTimeout(()=>this.modalSpinnerTimeout(), maxWaitingTimeMs);
    }
    this.socket.send(data);
  }
  
  public registerWebsocketMessageTypes(listener: WebsocketMessageListener, ...messageTypes: number[]): void {
    messageTypes.forEach(messageType => {
      let arr=this.messageType2listener.get(messageType)
      if(!arr){
        arr=[];
        this.messageType2listener.set(messageType, arr);
      }
      arr.push(listener);
    });
  }
  
  private log(message:string){
    let msg = document.createElement('p');
    if (message.startsWith("I")) {
      msg.className = 'info';
    } else if (message.startsWith("W")) {
      msg.className = 'warn';
    } else {
      msg.className = 'error';
    }
    msg.innerHTML = message.replace(ANSI_ESCAPE, "");
    this.scroller.insertBefore(msg, this.anchor);
    this.messageCount++;
    if (this.messageCount > MAX_MESSAGE_COUNT) {
      this.scroller.removeChild(this.scroller.firstChild!);
    }
  }

  private onWebsocketData(data: ArrayBuffer) {
    let arr=new Uint8Array(data);
    let bb = new flatbuffers.ByteBuffer(arr);
    let messageWrapper= MessageWrapper.getRootAsMessageWrapper(bb);
    console.log(`A message of type ${messageWrapper.messageType()} with length ${data.byteLength} has arrived.`);
    if(messageWrapper.messageType()==this.messageToUnlock){
      clearTimeout(this.modalSpinnerInterval);
      this.messageToUnlock=Message.NONE;
      this.setModal(false);
    }
    this.messageType2listener.get(messageWrapper.messageType())?.forEach((v)=>{
      v.onMessage(messageWrapper);
    });
  }

  private setModal(state:boolean){
    this.modal.style.display=state?"flex":"none";
  }

  onMessage(messageWrapper: MessageWrapper): void {
    let li=<LiveLogItem>messageWrapper.message(new LiveLogItem());
    this.log(<string>li.text());
  }

  public startup() {
    this.AddScreenController("home", WeblogScreenController);
    this.AddScreenController("wifimanager", WifimanagerController);
    this.AddScreenController("systemsettings", SystemScreenController);
    this.AddScreenController("usersettings", UsersettingsController);
    this.activateScreen("home");
    this.dialogController.init();

    try {
      console.log(`IConnecting to ${WS_URL}`);
      
      this.socket.binaryType="arraybuffer";
      this.socket.onerror = (event: Event) => { console.error('ESocketError'); };
      this.socket.onopen = (event) => { console.log('ISocket.open'); };
      this.socket.onmessage = (event:MessageEvent<any>) => { this.onWebsocketData(event.data); };
    } catch (e) {
      console.error('E ' + e);
    }
  }  
}

let app: AppController;
document.addEventListener("DOMContentLoaded", (e) => {
  app = new AppController();
  app.startup();
});