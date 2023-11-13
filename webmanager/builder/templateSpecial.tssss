import * as stream from "node:stream";
import * as fs from "node:fs"
import Vinyl from "vinyl";

export class SearchReplace{
  constructor(public search:string, public replaceFilePath:string){}
}

export default function templateSpecial(templatePath: string, defaultSearch:string, furtherSearchReplace:SearchReplace[]) {
  var htmlTemplate:string = fs.readFileSync(templatePath, {encoding:"utf-8"});
  furtherSearchReplace.forEach(e => {
    e.replaceFilePath=fs.readFileSync(e.replaceFilePath, {encoding:"utf-8"});
  });
  return new stream.Transform(
    {
      objectMode: true,
      transform(file: Vinyl, encoding: BufferEncoding, callback: stream.TransformCallback): void {
        if(!file.isBuffer()){
          callback(new Error("Only Buffers are acceptable"));
          return;
        }
        var content =  htmlTemplate.replace(defaultSearch, file.contents.toString(encoding));
        furtherSearchReplace.forEach(e => {
          content=content.replace(e.search, e.replaceFilePath);
        });  
        file.contents =  Buffer.from(content);
        if(file.basename.endsWith(".inc")){
          file.basename=file.basename.replace(".inc", "");
        }
        return callback(null, file);
      }
    });

};