
export interface Location2D {
    x: number;
    y: number;
}

export interface KeyValueTuple {
    key: string;
    value: any;
}

export class StringNumberTuple{
    public constructor(public s:string, public n:number){}
}

export default function(selectorForSingleElement:string){ return document.querySelector(selectorForSingleElement) as HTMLElement};

export class U
{
    public static readonly SVGNS = "http://www.w3.org/2000/svg";
    public static readonly XLINKNS = "http://www.w3.org/1999/xlink";
    public static readonly HTMLNS = "http://www.w3.org/1999/xhtml";
    public static  UNIT_SEPARATOR = '\x1F';
    public static  RECORD_SEPARATOR = '\x1E';
    public static  GROUP_SEPARATOR = '\x1D';
    public static  FILE_SEPARATOR = '\x1C';

    public static $$(selectorForMultipleElements:string){
        return document.querySelectorAll(selectorForMultipleElements);
    }

    public static T(parent:Element, templateId:string):Element{
        const template = document.querySelector(`template#${templateId}`) as HTMLTemplateElement;
        let instance= document.importNode(template.content, true);
        let child = parent.appendChild(instance);
        return parent.lastElementChild as HTMLElement;
    }
    /**
     * Add a new HTML Element
     * @param parent the parent of the new element
     * @param type
     * @param classes 
     * @param textContent 
     * @returns 
     */
    public static A(parent: HTMLElement, type:"tr"|"td"|"a"|"div"|"option"|"select",  classes?: string[], textContent?:string){
        return parent.appendChild(U.E(U.HTMLNS, type, classes, textContent)) as HTMLElement;
    }
    public static E(ns:string, type:string, classes?: string[], textContent?:string)
    {
        let element = document.createElementNS(ns, type);
        classes?.forEach(clazz=>element.classList.add(clazz));
        element.textContent=textContent??"";
        return element; 
    }

    public static EventCoordinatesInSVG(evt:MouseEvent, element:Element, positionRatio:number=1):Location2D {
        let rect = element.getBoundingClientRect();
        return {x: (evt.clientX - rect.left)/positionRatio, y:(evt.clientY - rect.top)/positionRatio}
    }

    public static GetAcceptedLanguages() {
        return navigator.languages || [navigator.language];
    };

    public static EscapeToVariableName(n:string){
        return n.toLocaleUpperCase().replace(" ", "_");
    }  
}