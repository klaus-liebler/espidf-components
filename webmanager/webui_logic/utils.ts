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

export const gel = (e:string) => document.getElementById(e)!;
export const gqs = (e:string) => document.querySelector(e) as HTMLElement;
export const gqsa = (cssSelector:string, fn:(value: HTMLElement, key: number, parent: NodeListOf<HTMLElement>) => void) => (document.querySelectorAll(cssSelector) as NodeListOf<HTMLElement>).forEach(fn);

export const $=(selectorForSingleElement:string)=>document.querySelector(selectorForSingleElement) as HTMLElement;
export const $$=(selectorForMultipleElements:string)=>document.querySelectorAll(selectorForMultipleElements);

export function EscapeToVariableName(n:string){
    return n.toLocaleUpperCase().replace(" ", "_");
} 

export const SVGNS = "http://www.w3.org/2000/svg";
export const  XLINKNS = "http://www.w3.org/1999/xlink";
export const  HTMLNS = "http://www.w3.org/1999/xhtml";

export function T(parent:Element, templateId:string):Element{
    const template = $(`template#${templateId}`) as HTMLTemplateElement;
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
export function A(parent: HTMLElement, type:"tr"|"td"|"a"|"div"|"option"|"select",  classes?: string[], textContent?:string){
    return parent.appendChild(E(HTMLNS, type, classes, textContent)) as HTMLElement;
}

/**
 * 
 * @param ns Create a new element without adding it 
 * @param type 
 * @param classes 
 * @param textContent 
 * @returns 
 */
export function E(ns:string, type:string, classes?: string[], textContent?:string)
{
    let element = document.createElementNS(ns, type);
    classes?.forEach(clazz=>element.classList.add(clazz));
    element.textContent=textContent??"";
    return element; 
}

export function EventCoordinatesInSVG(evt:MouseEvent, element:Element, positionRatio:number=1):Location2D {
    let rect = element.getBoundingClientRect();
    return {x: (evt.clientX - rect.left)/positionRatio, y:(evt.clientY - rect.top)/positionRatio}
}

export function GetAcceptedLanguages() {
    return navigator.languages || [navigator.language];
};

 

export function Svg(parent: Element, type:string,  attributes:string[], classes?: string[]):SVGElement {
        return  parent.appendChild(<SVGElement>Elem(SVGNS, type, attributes, classes));
    }

//<svg class="icon icon-wastebasket"><use xlink:href="#icon-wastebasket"></use></svg>
export function SvgIcon(parent: Element, iconname:string):SVGSVGElement
{
    let svg = <SVGSVGElement>Svg(parent, "svg", [], ["icon", "icon-"+iconname]);
    let use =Svg(svg, "use", [], []);
    use.setAttributeNS(XLINKNS, "href", "#icon-"+iconname);
    parent.appendChild(svg);
    return svg;
}

export function ColorNumColor2ColorDomString(num:number):string {
    num=num>>8; //as format is RGBA
    let str = num.toString(16);
    while (str.length < 6) str = "0" + num;
    return "#"+str;
}

export function ColorDomString2ColorNum(colorString: string):number {
    return parseInt(colorString.substring(1), 16);
}

export function Html(parent: Element, type:string,  attributes:string[]=[], classes: string[]=[], textContent:string=""):HTMLElement {
    return parent.appendChild(<HTMLElement>Elem(HTMLNS, type, attributes, classes, textContent));
}

export function HtmlAsFirstChild(parent: Element, type:string,  attributes:string[], classes?: string[], textContent?:string):HTMLElement {
    if(parent.firstChild)
        return parent.insertBefore(<HTMLElement>Elem(HTMLNS, type, attributes, classes, textContent), parent.firstChild);
    else
        return parent.appendChild(<HTMLElement>Elem(HTMLNS, type, attributes, classes, textContent));
}

export function Elem(ns:string, type:string, attributes:string[], classes?: string[], textContent?:string):Element
{
    let element = document.createElementNS(ns, type);
    if(classes)
    {
        for (const clazz of classes) {
            element.classList.add(clazz);
        }
    }
    if(attributes){
        for(let i=0;i<attributes.length;i+=2)
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

