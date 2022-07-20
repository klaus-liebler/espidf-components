//Configuration
import $ from './utils'
import {U} from './utils'
import Config from './config'
import { ConfigGroup, ConfigItemRT, ValueUpdater } from './interfaces_baseclasses';
const MYURL = "http://192.168.210.0/configmanager";

class AppController implements ValueUpdater{
    UpdateString(key: string, value: string): void {
        console.log(`${key}=${value}`);
    }
    UpdateInteger(key: string, value: number): void {
        console.log(`${key}=${value}`);
    }
    UpdateFloat(key: string, value: number): void {
        console.log(`${key}=${value}`);
    }
    UpdateBoolean(key: string, value: boolean): void {
        console.log(`${key}=${value}`);
    }
    UpdateEnum(key: string, selectedIndex: number): void {
        console.log(`${key}=${selectedIndex}`);
    }

    private main_section = $("#main") as HTMLElement;

    private  key2configItemRT = new Map<string, ConfigItemRT>();

    public async startup():Promise<void>{

        $("#btn_update").onclick=()=>{this.sendToServer()};
        
        let cfg:ConfigGroup[] = new Config().Build();
        for(const group of cfg){
            let acc_pan=U.T(this.main_section, "template_accordion");
            acc_pan.children[0].textContent=group.name;
            let pan=acc_pan.children[1]!.children[0] as HTMLTableElement;
            for(const item of group.items){
                let inputElement = item.RenderHtml(pan, this);
                this.key2configItemRT.set(item.displayName, new ConfigItemRT(item, inputElement));
            }
        }
        for (const elem of U.$$(".accordion>button") as NodeListOf<HTMLElement>) {
            elem.onclick = ()=>{
                elem.classList.toggle("active");
                let panel = elem.nextElementSibling as HTMLElement;
                if (panel.style.display === "block") {
                    panel.style.display = "none";
                } else {
                    panel.style.display = "block";
                }
            }
        }
        let response = await fetch(MYURL,{
            method:"POST",
            headers: {
              "Content-Type": "text/plain",
            },
            body: "",
    
          });
          let txt = await response.text();
          this.parseData(txt);
          

    }

    async sendToServer() {
        let s="";
        this.key2configItemRT.forEach((value, key)=>{
            s+=key;
            s+=U.UNIT_SEPARATOR;
            s+=value.cfgItem.GetValueAsString(value.inputElement)
            s+=U.RECORD_SEPARATOR;
        });
        console.log(s);
        await fetch(MYURL,{
            method:"POST",
            headers: {
              "Content-Type": "text/plain",
            },
            body: s,
    
          });
    }
    
    parseData(txt: string) {
        let items = txt.split(U.RECORD_SEPARATOR).filter(r => r !== "");
        items.forEach((str)=>{
            let items = str.split(U.UNIT_SEPARATOR);
            let key=items[0];
            let ci=this.key2configItemRT.get(key);
            if(ci===undefined) return;
            ci.cfgItem.ParseValueFromStringAndSetValueInDOM(items[1], ci.inputElement)
        });
    }
}

let app: AppController;
document.addEventListener("DOMContentLoaded", (e) => {
    app = new AppController();
    app.startup();
});


