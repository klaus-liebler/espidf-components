
export function EscapeToVariableName(n:string){
    return n.toLocaleUpperCase().replace(" ", "_");
} 

export abstract class ConfigItem{
    constructor(public readonly displayName:string){}
    abstract RenderCPP(codeBuilder:string):string;
}

export class StringItem extends ConfigItem{
    constructor(displayName:string, public readonly def:string="", public readonly regex:RegExp=/.*/){super(displayName)}
    
    RenderCPP(codeBuilder: string): string {
        let esc=EscapeToVariableName(this.displayName);
        codeBuilder+=`char* Get${esc}(){return GetString(${esc}, "${this.def}");}\n`;
        return codeBuilder;
    }

}
export class IntegerItem extends ConfigItem{
    constructor(displayName:string, public readonly def:number=0, public readonly min:number=0, public readonly max:number=Number.MAX_SAFE_INTEGER, public readonly step:number=1){
        super(displayName)
    }
    
    RenderCPP(codeBuilder: string): string {
        codeBuilder+="IntegerItem "+this.displayName;
        return codeBuilder
    }
}
export class BooleanItem extends ConfigItem{
    
    RenderCPP(codeBuilder: string): string {
        codeBuilder+="BooleanItem "+this.displayName;
        return codeBuilder;
    }
    constructor(displayName:string, public readonly def:boolean=false){
        super(displayName);
    }

}

export class EnumItem extends ConfigItem{
   
    RenderCPP(codeBuilder: string): string {
        codeBuilder+="EnumItem "+this.displayName;
        return codeBuilder;
    }
    constructor(displayName:string, public readonly values:string[]){
        super(displayName);
    }
}

export class ConfigGroup{
    constructor(public readonly name:string, public items:ConfigItem[]){}
}