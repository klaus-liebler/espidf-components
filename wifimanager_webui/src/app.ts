//const DATAURL = "http://labathome-ebe548/wifimanager";
const DATAURL = "wifimanager";
//const DATAURL = "http://192.168.210.0/wifimanager";

const gel = (e:string) => document.getElementById(e)!;
const gqs = (e:string) => document.querySelector(e) as HTMLElement;
const gqsa = (cssSelector:string, fn:(value: HTMLElement, key: number, parent: NodeListOf<HTMLElement>) => void) => (document.querySelectorAll(cssSelector) as NodeListOf<HTMLElement>).forEach(fn);
const showScreen= (e:HTMLElement) => {
  (document.querySelectorAll("#screens>article") as NodeListOf<HTMLElement>).forEach((e)=>e.style.display="none");
  e.style.display = "block";
}
class $
{
    public static readonly SVGNS = "http://www.w3.org/2000/svg";
    public static readonly XLINKNS = "http://www.w3.org/1999/xlink";
    public static readonly HTMLNS = "http://www.w3.org/1999/xhtml";

    public static Svg(parent: Element, type:string,  attributes:string[], classes?: string[]):SVGElement {
        return  parent.appendChild(<SVGElement>$.Elem($.SVGNS, type, attributes, classes));
    }

    public static Html(parent: Element, type:string, attributes?:string[], classes?: string[], textContent?:string):HTMLElement {
        return parent.appendChild(<HTMLElement>$.Elem($.HTMLNS, type, attributes, classes, textContent));
    }

    public static HtmlAsFirstChild(parent: Element, type:string,  attributes?:string[], classes?: string[], textContent?:string):HTMLElement {
        if(parent.firstChild)
            return parent.insertBefore(<HTMLElement>$.Elem($.HTMLNS, type, attributes, classes, textContent), parent.firstChild);
        else
            return parent.appendChild(<HTMLElement>$.Elem($.HTMLNS, type, attributes, classes, textContent));
    }

    private static Elem(ns:string, type:string, attributes?:string[], classes?: string[], textContent?:string):Element
    {
        let element = document.createElementNS(ns, type);
        if(classes)
        {
            for (const clazz of classes) {
                element.classList.add(clazz);
            }
        }
        let i:number;
        if(attributes){
            for(i=0;i<attributes.length;i+=2)
            {
                element.setAttribute(attributes[i], attributes[i+1]);
            }
        }
        if(textContent)
        {
            element.textContent=textContent;
        }
        return element;
    }
}

const UNIT_SEPARATOR = '\x1F';
const RECORD_SEPARATOR = '\x1E';
const GROUP_SEPARATOR = '\x1D';
const FILE_SEPARATOR = '\x1C';

class AccessPoint{
  constructor(public ssid:string, public primaryChannel:number, public rssi:number, public authMode:number){}

  public static ParseFromString(str:string):AccessPoint {
    let items = str.split(UNIT_SEPARATOR);
    return new AccessPoint(items[0], parseInt(items[1]), parseInt(items[2]), parseInt(items[3]));
  }
}

class AppController {

    private screen_main = gel("main");
    private screen_enter_password = gel("enter_password");
    private screen_connect_wait = gel("connect_wait");
    private screen_connection_details = gel("connection_details");
    private screen_connect_success = gel("connection_success");
    private screen_connect_failed = gel("connection_failed");
    private diag_confirm_delete = gel("confirm_delete");
    private screen_delete_success = gel("delete_success");
    private selectedSSID = "";
    private refreshDataInterval:any=null;

    constructor() {}

    private cancel() {
      this.selectedSSID = "";
      showScreen(this.screen_main);
    }
  
    private stopRefreshDataInterval() {
      if (this.refreshDataInterval != null) {
        clearInterval(this.refreshDataInterval);
        this.refreshDataInterval = null;
      }
    }
    
    private startRefreshDataInterval() {
      this.refreshDataInterval = setInterval(()=>{this.refreshData();}, 4000);
    }

    public startup() {
      showScreen(this.screen_main);
      gel("btnShowDetails").onclick=()=>showScreen(this.screen_connection_details);
      let pwdPassword =(gel("pwdPassword") as HTMLInputElement);
      gel("btnJoin").onclick=()=>this.performConnect(pwdPassword.value);
      pwdPassword.onkeydown = (e)=>{
        if(e.key=="Enter"){
          this.performConnect(pwdPassword.value);
        }
      }
      
      (document.querySelectorAll("input[type='button'].cancel") as NodeListOf<HTMLButtonElement>).forEach((e)=>e.onclick=()=>this.cancel());
      
      gel("btnDisconnectFromMain").onclick=
      gel("btnDisconnectFromDetails").onclick=
      ()=>{
        this.diag_confirm_delete.style.display="block";
        this.screen_main.classList.add("blur");
        this.screen_connection_details.classList.add("blur")
      };
      
      gel("no-disconnect").onclick=() => {
        this.diag_confirm_delete.style.display = "none";
        this.screen_main.classList.remove("blur");
        this.screen_connection_details.classList.remove("blur");
      };
      
      gel("yes-disconnect").onclick=() => {
        this.stopRefreshDataInterval();
        this.selectedSSID = "";
    
        this.diag_confirm_delete.style.display = "none";
        this.screen_main.classList.remove("blur");
        this.screen_connection_details.classList.remove("blur");
    
        fetch(DATAURL, {
          method: "DELETE",
        }).then((response)=>{
          if(!response.ok){
            console.log(response.statusText);
          }
          showScreen(this.screen_delete_success);
        })
      };
      this.refreshData();
      this.startRefreshDataInterval();
    }

    private performConnect(password:string) {
      //stop the status refresh. This prevents a race condition where a status
      //request would be refreshed with wrong ip info from a previous connection
      //and the request would automatically shows as succesful.
      this.stopRefreshDataInterval();
      showScreen(this.screen_connect_wait);
      let content=`${this.selectedSSID}`+UNIT_SEPARATOR+`${password}`+UNIT_SEPARATOR+RECORD_SEPARATOR;
      console.log("Sending connect request with "+content)
      fetch(DATAURL, {
        method: "POST",
        headers: {
          "Content-Type": "text/plain",
        },
        body: content,
      });
      this.startRefreshDataInterval();
    }

    public processStatusString(statusString:string){
      let items = statusString.split(UNIT_SEPARATOR);
      let ssid_ap=items[0];
      let hostname= items[1]
      let ssid = items[2];
      let rssi = parseInt(items[3]);
      let ip=items[4];
      let netmask=items[5];
      let gw=items[6];
      let urc=parseInt(items[7]);
      if(ssid!=""){
        gqsa(".current_hostname",(e)=>e.textContent = hostname);
        gqsa(".current_rssi", (e)=>e.textContent = rssi+"dB");
        (gel("btnDisconnectFromMain") as HTMLInputElement).disabled=
        (gel("btnDisconnectFromDetails")as HTMLInputElement).disabled=false;
        gqsa(".link2site", (e)=>{
          (e as HTMLAnchorElement).href = `http://${hostname}`;
          (e as HTMLAnchorElement).textContent = `http://${hostname}`;
        });
      }else{
        gqsa(".current_hostname",(e)=>e.textContent = "");
        gqsa(".current_rssi", (e)=>e.textContent = "");
        (gel("btnDisconnectFromMain") as HTMLInputElement).disabled=
        (gel("btnDisconnectFromDetails")as HTMLInputElement).disabled=true;
      }
      gqsa(".current_ssid_ap", (e)=>e.textContent=ssid_ap);
      gqsa(".current_ssid", (e)=>e.textContent=ssid);
      gqsa(".current_ip", (e)=>e.textContent = ip);
      gqsa(".current_netmask", (e)=>e.textContent = netmask);
      gqsa(".current_gw", (e)=>e.textContent = gw);

      switch(urc){
        case 0:
          break;
        case 1:
          console.info("Got connection!");
          showScreen(this.screen_connect_success);
          break;
        case 2:
          console.info("Connection attempt failed!");
          showScreen(this.screen_connect_failed);
          break;
        case 3:
          console.log("Manual disconnect requested...");
          showScreen(this.screen_main);
          break;
        case 4:
          showScreen(this.screen_main);
          break;
      }
    }

    private newSSIDSelected(ssid:string){
      this.selectedSSID=ssid;
      document.querySelectorAll(".ssid_to_connect_to").forEach(e=>e.textContent=ssid);
      showScreen(this.screen_enter_password);
    }

    public processAccessPointsStrings(apStrings:string[]){

      let access_points = new Map<string, AccessPoint>();
      for (const str of apStrings) {
        //make unique
        let ap_new:AccessPoint = AccessPoint.ParseFromString(str);
        let key = ap_new.ssid+"_"+ap_new.authMode;
        let ap_exist = access_points.get(key);
        if(ap_exist===undefined){
          access_points.set(key, ap_new);
        }
        else{
          ap_exist.rssi=Math.max(ap_exist.rssi, ap_new.rssi);
        }
      }
      let access_points_list = [...access_points.values()];
      access_points_list.sort((a, b) => {
        //sort according to rssi
        var x = a.rssi;
        var y = b.rssi;
        return x < y ? 1 : x > y ? -1 : 0;
      });
      let table = gel("wifi-list") as HTMLElement;
      table.textContent="";
      
      (gel("btnManualConnect") as HTMLInputElement).onclick=(e)=>this.newSSIDSelected((gel("inpManualConnect") as HTMLInputElement).value);
      const icon_lock_template = document.getElementById("icon-lock") as HTMLTemplateElement;
      const icon_rssi_template = document.getElementById("icon-wifi") as HTMLTemplateElement;
      access_points_list.forEach((e, idx, array)=>{
        let tr= $.Html(table, "tr");
        let td_rssi = $.Html(tr, "td");
        let figure_rssi= document.importNode(icon_rssi_template.content, true);
        td_rssi.appendChild(figure_rssi);
        let td_auth = $.Html(tr, "td");
        if(e.authMode!= 0){
          td_auth.appendChild(document.importNode(icon_lock_template.content, true));
        }
        let rssiIcon= td_rssi.children[0];
        (rssiIcon.children[0] as SVGPathElement).style.fill= e.rssi>=-60?"black":"grey";
        (rssiIcon.children[1] as SVGPathElement).style.fill= e.rssi>=-67?"black":"grey";
        (rssiIcon.children[2] as SVGPathElement).style.fill= e.rssi>=-75?"black":"grey";
        $.Html(tr, "td", [], [], `${e.ssid} [${e.rssi}dB]`);
        tr.onclick=()=>this.newSSIDSelected(e.ssid);
      }); 
    }


    private refreshData(){
      fetch(DATAURL,{
        method:"POST",
        headers: {
          "Content-Type": "text/plain",
        },
        body: "",
      })
      .then((response)=>response.text()
      )
      .then((txt)=>{
        if(txt.length==0){
          console.error("Received an empty string from server");
          return;
        }
        let items = txt.split(RECORD_SEPARATOR).filter(r => r !== "");
        this.processStatusString(items[0]);
        this.processAccessPointsStrings(items.slice(1));
      }
      )
      .catch((e)=>console.log(e));
    }
}

let app: AppController;
document.addEventListener("DOMContentLoaded", (e) => {
    app = new AppController();
    app.startup();
});


