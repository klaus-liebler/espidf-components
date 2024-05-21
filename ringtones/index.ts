import * as p from './rtttlparser'
import path from "node:path"
import * as fs from "node:fs"
import { Song } from './rtttlparser';
import { CasingMode, s2i } from './string2identifierName';
const directoryPath = path.join("./", 'rtttl_data_files');


var songs: Array<Song> = []
fs.readdirSync(directoryPath).forEach((filename) => {
  const rtttl = fs.readFileSync(path.join(directoryPath, filename))
  songs.push(p.parse(rtttl.toString()));

})

let output = "#pragma once\n"
for (const s of songs) {
  output += `constexpr Note ${s2i(s.Name, CasingMode.UPPER)}[] = {`
  for (const note of s.Tones) {
    output += `{${note.Frequency.toFixed(0)},${note.Duration}},`;
  }
  output += "END_OF_SONG};\n";
}
output += "\n\n";

output += "const Note *RINGTONE_SOUNDS[] = {0,"
for (const s of songs) {
 output+=`${s2i(s.Name, CasingMode.UPPER)},`
} 
output += "};\n";
output += "enum class RINGTONE_SONG{UNDEFINED,"
for (const s of songs) {
  output+=`${s2i(s.Name, CasingMode.UPPER)},`
 } 
output += "};"
console.log(output);
fs.writeFileSync("./songs.hh.inc", output, {flag:"w"})