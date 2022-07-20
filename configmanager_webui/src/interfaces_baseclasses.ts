import $ from './utils'
import {U} from './utils'

export abstract class ConfigItem{
    constructor(public readonly displayName:string){}
    abstract RenderHtml(parent:HTMLElement, callback:ValueUpdater):HTMLInputElement|HTMLSelectElement;
    abstract RenderCPP(codeBuilder:string):void;
    abstract GetValueAsString(element:HTMLInputElement|HTMLSelectElement):string;
    abstract  ParseValueFromStringAndSetValueInDOM(parseText:string, element:HTMLInputElement|HTMLSelectElement):void;
}

export interface ValueUpdater{
    UpdateString(key:string, value:string):void;
    UpdateInteger(key:string, value:number):void;
    UpdateFloat(key:string, value:number):void;
    UpdateBoolean(key:string, value:boolean):void;
    UpdateEnum(key:string, selectedIndex:number):void;
}

//RuntimeItem verknüft das ConfigItem mit dem input-Element in der UI

export class ConfigItemRT{
    constructor(public readonly cfgItem:ConfigItem, public readonly inputElement:HTMLInputElement|HTMLSelectElement){
    }
}


export class StringItem extends ConfigItem{
    constructor(displayName:string, public readonly def:string="", public readonly regex:RegExp=/.*/){super(displayName)}
    RenderHtml(parent: HTMLElement, callback:ValueUpdater): HTMLInputElement {
        let tr=U.T(parent, "StringItem") as HTMLTableRowElement;
        tr.firstElementChild!.textContent=this.displayName;
        let inputElement = tr.children[1]!.children[0] as HTMLInputElement;
        inputElement.setAttribute("value", this.def);
        inputElement.setAttribute("pattern", this.regex.source);
        inputElement.onchange=()=>callback.UpdateString(U.EscapeToVariableName(this.displayName), inputElement.value);
        return inputElement;
    }
    RenderCPP(codeBuilder: string): void {
        let esc=U.EscapeToVariableName(this.displayName);
        codeBuilder+=`char* Get${esc}(){return GetString(${esc}, "${this.def}");}\n`;
    }

    ParseValueFromStringAndSetValueInDOM(parseText:string, element:HTMLInputElement){
        element.value=parseText;
    }

    GetValueAsString(element:HTMLInputElement|HTMLSelectElement){
        return element.value;
    }
}
export class IntegerItem extends ConfigItem{
    constructor(displayName:string, public readonly def:number=0, public readonly min:number=0, public readonly max:number=Number.MAX_SAFE_INTEGER, public readonly step:number=1){
        super(displayName)
    }
    RenderHtml(parent: HTMLElement, callback: ValueUpdater): HTMLInputElement {
        let tr=U.T(parent, "IntegerItem") as HTMLTableRowElement;
        tr.firstElementChild!.textContent=this.displayName;
        let inputElement = tr.children[1]!.children[0] as HTMLInputElement;
        inputElement.setAttribute("value", this.def.toString());
        inputElement.setAttribute("value", this.def.toString());
        inputElement.setAttribute("min", this.min.toString());
        inputElement.setAttribute("max", this.max.toString());
        inputElement.onchange=()=>callback.UpdateInteger(U.EscapeToVariableName(this.displayName), parseInt(inputElement.value));
        return inputElement;
    }
    RenderCPP(codeBuilder: string): void {
        throw new Error('Method not implemented.');
    }

    ParseValueFromStringAndSetValueInDOM(parseText:string, element:HTMLInputElement){
        element.value=parseText;
    }

    GetValueAsString(element:HTMLInputElement|HTMLSelectElement){
        return element.value;
    }
}
export class BooleanItem extends ConfigItem{
    RenderHtml(parent: HTMLElement, callback: ValueUpdater): HTMLInputElement {
        let tr=U.T(parent, "BooleanItem") as HTMLTableRowElement;
        tr.firstElementChild!.textContent=this.displayName;
        let inputElement = tr.children[1]!.children[0] as HTMLInputElement;
        inputElement.checked=this.def;
        inputElement.onchange=()=>callback.UpdateBoolean(U.EscapeToVariableName(this.displayName), inputElement.checked);
        return inputElement;
    }
    RenderCPP(codeBuilder: string): void {
        throw new Error('Method not implemented.');
    }
    constructor(displayName:string, public readonly def:boolean=false){
        super(displayName);
    }

    ParseValueFromStringAndSetValueInDOM(parseText:string, element:HTMLInputElement){
        (element as HTMLInputElement).checked=parseText=="true";
    }

    GetValueAsString(element:HTMLInputElement){
        return element.checked+"";
    }
}

export class EnumItem extends ConfigItem{
    RenderHtml(parent: HTMLElement, callback: ValueUpdater): HTMLSelectElement {
        let tr=U.T(parent, "EnumItem") as HTMLTableRowElement;
        tr.firstElementChild!.textContent=this.displayName;
        let selectElement = tr.children[1]!.children[0] as HTMLSelectElement;
        this.values.forEach((v,i)=>{
            let optionElement = U.A(selectElement, "option", undefined, v);
            optionElement.setAttribute("value", i.toString());
        });
        selectElement.onchange=()=>callback.UpdateEnum(U.EscapeToVariableName(this.displayName), selectElement.selectedIndex);
        return selectElement;
    }
    RenderCPP(codeBuilder: string): void {
        throw new Error('Method not implemented.');
    }
    constructor(displayName:string, public readonly values:string[]){
        super(displayName);
    }

    ParseValueFromStringAndSetValueInDOM(parseText:string, element:HTMLSelectElement){
        element.selectedIndex=parseInt(parseText);
    }

    GetValueAsString(element:HTMLSelectElement):string{
        return element.selectedIndex.toString();
    }
}

export class ConfigGroup{
    constructor(public readonly name:string, public items:ConfigItem[]){}
}