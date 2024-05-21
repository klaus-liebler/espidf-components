export enum CasingMode{
    ORIGINAL,
    LOWER,
    UPPER,
}

export function s2i(s:string, cm:CasingMode=CasingMode.UPPER){
    if(cm==CasingMode.LOWER)
        s=s.toLocaleLowerCase()
    else if(cm==CasingMode.UPPER){
        s=s.toLocaleUpperCase()
    }
    //remove leading and trailing whitespace
    s=s.trimStart().trimEnd()
    //Make spaces into underscores
    s = s.replace(/[\\s\\t\\n]+'/, '_')
    //Remove invalid characters
    s = s.replace(/[^0-9a-zA-Z_]/, '')
    return s
}