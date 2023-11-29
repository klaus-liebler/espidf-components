import { RequestGetUserSettings, RequestSetUserSettings, ResponseGetUserSettings, ResponseSetUserSettings, SettingWrapper } from "./flatbuffers_gen/webmanager";
import { Message } from "./flatbuffers_gen/webmanager/message";
import { MessageWrapper } from "./flatbuffers_gen/webmanager/message-wrapper";
import { ScreenController } from "./screen_controller";
import * as flatbuffers from 'flatbuffers';
import us from "./usersettings_copied_during_build";
import { BooleanItem, ConfigGroup, ConfigItemRT, EnumItem, IntegerItem, ItemState, StringItem, ValueUpdater } from "./usersettings_base";
import { T } from "./utils";
import { Severrity } from "./dialog_controller";

class ConfigGroupRT{
    constructor(public groupName:string, public saveButton:HTMLInputElement, public updateButton:HTMLInputElement){}
}


export class UsersettingsController extends ScreenController implements ValueUpdater{

    UpdateSaveButton(groupName:string){
        let group=this.groupName2itemName2configItemRT.get(groupName);
        let atLeastOneHasChanged=false;
        for(let v of group!.values()){
            if(v.cfgItem.HasAChangedValue()){
                atLeastOneHasChanged=true;
                break;
            }
        }
        let gc=this.groupName2configGroupRT.get(groupName)!;
        gc.saveButton.disabled=!atLeastOneHasChanged;
    }

    UpdateString(groupName:string, i:StringItem, v:string): void {
        console.log(`${i.displayName}=${v}`);
        this.UpdateSaveButton(groupName);
       
    }
    UpdateInteger(groupName:string, i:IntegerItem, v: number): void {
        console.log(`${i.displayName}=${v}`);
        this.UpdateSaveButton(groupName);
    }

    UpdateBoolean(groupName:string, i: BooleanItem, value: boolean): void {
        console.log(`${i.displayName}=${value}`);
        this.UpdateSaveButton(groupName);
    }
    UpdateEnum(groupName:string, i: EnumItem, selectedIndex: number): void {
        console.log(`${i.displayName}=${selectedIndex}`);
        this.UpdateSaveButton(groupName);
    }

    private  groupName2itemName2configItemRT = new Map<string, Map<string,ConfigItemRT>>();
    private  groupName2configGroupRT = new Map<string, ConfigGroupRT>();


    private sendRequestGetUserSettings(groupName:string) {
        let b = new flatbuffers.Builder(256);
        let n = RequestGetUserSettings.createRequestGetUserSettings(b, b.createString(groupName));
        let mw = MessageWrapper.createMessageWrapper(b, Message.RequestGetUserSettings, n);
        b.finish(mw);
        this.appManagement.sendWebsocketMessage(b.asUint8Array(), [Message.ResponseGetUserSettings], 3000);
    }

    sendRequestSetUserSettings(groupName: string) {
        let b = new flatbuffers.Builder(1024);
        let vectorOfSettings:number[]=[];
        for(let v of this.groupName2itemName2configItemRT.get(groupName)!.values()){
            vectorOfSettings.push(v.cfgItem.WriteToFlatbufferBufferAndReturnSettingWrapperOffset(b));
        }
        let settingsOffset:number= ResponseGetUserSettings.createSettingsVector(b, vectorOfSettings)
        let n = RequestSetUserSettings.createRequestSetUserSettings(b, b.createString(groupName), settingsOffset);
        let mw = MessageWrapper.createMessageWrapper(b, Message.RequestSetUserSettings, n);
        b.finish(mw);
        this.appManagement.sendWebsocketMessage(b.asUint8Array(), [Message.ResponseSetUserSettings], 3000);
    }

    

    onMessage(messageWrapper: MessageWrapper): void {
        switch (messageWrapper.messageType()) {
            case Message.ResponseGetUserSettings:
                this.onResponseGetUserSettings(messageWrapper);
                break;
            case Message.ResponseSetUserSettings:
                this.onResponseSetUserSettings(messageWrapper);
            default:
                break;
        }
    }
    onResponseSetUserSettings(messageWrapper: MessageWrapper): void{
        let res = <ResponseSetUserSettings>messageWrapper.message(new ResponseSetUserSettings());
        let group=this.groupName2itemName2configItemRT.get(res.groupName()!);
        if(!group){
            this.appManagement.DialogController().showOKDialog(Severrity.WARN, `Received confirmation for unknown group name ${res.groupName()}`);
            return;
        }
        group.forEach((v,k,m)=>{v.Flag=false});
        let unknownNames:string[]=[];
        for (let i = 0; i < res.settingsLength(); i++) {
            let name = res.settings(i);
            let rtItem = group.get(name);
            if(!rtItem){
                unknownNames.push(name);
                continue;
            }
            rtItem.cfgItem.ConfirmSuccessfulWrite();
            rtItem.Flag=true;
        }
        let nonStoredEntries:string[]=[];
        group.forEach((v,k,m)=>{
            if(!v.Flag){
                nonStoredEntries.push(v.cfgItem.displayName);
            }
        });
        if(unknownNames.length!=0 || nonStoredEntries.length!=0){
            this.appManagement.DialogController().showOKDialog(Severrity.WARN, `The following errors occured while receiving data for ${res.groupName()}: Unknown names: ${unknownNames.join(", ")}; No successful storage for: ${nonStoredEntries.join(", ")};`);
        }
    }
    
    onResponseGetUserSettings(messageWrapper: MessageWrapper): void{
        let sd = <ResponseGetUserSettings>messageWrapper.message(new ResponseGetUserSettings());
        let group=this.groupName2itemName2configItemRT.get(sd.groupName()!);
        if(!group){
            this.appManagement.DialogController().showOKDialog(Severrity.WARN, `Received settings for unknown group name ${sd.groupName()}`);
            return;
        }
        group.forEach((v,k,m)=>{v.Flag=false});
        let unknownNames:string[]=[];
        for (let i = 0; i < sd.settingsLength(); i++) {
            let name = sd.settings(i)!.name()!;
            let rtItem = group.get(name);
            if(!rtItem){
                unknownNames.push(name);
                continue;
            }
            rtItem.cfgItem.ReadFlatbuffersObjectAndSetValueInDom(sd.settings(i)!)
            rtItem.Flag=true;
        }
        let nonUpdatedEntries:string[]=[];
        group.forEach((v,k,m)=>{
            if(!v.Flag){
                nonUpdatedEntries.push(v.cfgItem.displayName);
                v.cfgItem.NoDataFromServerAvailable();
            }
        });
        if(unknownNames.length!=0 || nonUpdatedEntries.length!=0){
            this.appManagement.DialogController().showOKDialog(Severrity.WARN, `The following errors occured while receiving data for ${sd.groupName()}: Unknown names: ${unknownNames.join(", ")}; No updates for: ${nonUpdatedEntries.join(", ")};`);
        }
    }

    
    onCreate(): void {
        this.appManagement.registerWebsocketMessageTypes(this, Message.ResponseGetUserSettings, Message.ResponseSetUserSettings);
        let cfg:ConfigGroup[] = us.Build();
        for(const group of cfg){
            let acc_pan=T(this.appManagement.MainElement(), "Accordion");
            let button:HTMLButtonElement=<HTMLButtonElement>acc_pan.children[0];
            button.children[0].textContent=group.name;
            button.onclick=()=>{
                button.classList.toggle("active");
                let panel = button.nextElementSibling as HTMLElement;
                if (panel.style.display === "block") {
                    panel.style.display = "none";
                } else {
                    panel.style.display = "block";
                    this.sendRequestGetUserSettings(group.name);
                }
            };
            let pan=acc_pan.children[1]!.children[0]!.children[1]! as HTMLTableSectionElement;
            for(const item of group.items){
                item.RenderHtml(pan, group.name, this);
                let itemName2configItemRT=this.groupName2itemName2configItemRT.get(group.name);
                if(!itemName2configItemRT){
                    itemName2configItemRT=new Map<string, ConfigItemRT>();
                    this.groupName2itemName2configItemRT.set(group.name, itemName2configItemRT);
                }
                itemName2configItemRT.set(item.displayName, new ConfigItemRT(item, group.name));
            }
            let saveButton = button.children[1] as HTMLInputElement;
            saveButton.disabled=true;
            saveButton.onclick=(e)=>{this.sendRequestSetUserSettings(group.name);e.stopPropagation()}
            let updateButton = button.children[2] as HTMLInputElement;
            updateButton.onclick=(e)=>{this.sendRequestGetUserSettings(group.name);e.stopPropagation()}
            this.groupName2configGroupRT.set(group.name, new ConfigGroupRT(group.name, saveButton, updateButton));
        }
    }
   
    onFirstStart(): void {
        
    }
    onRestart(): void {
        
    }
    onPause(): void {
    }

}
