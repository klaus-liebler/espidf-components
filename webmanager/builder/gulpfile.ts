const debug = false;

var comPort="COM3";
var hostnamePrefix= "esp32host_";


/*
//Build is complex
1.) Only once!: Call gulp rootCA (creates a root certificate+private key rootCA.cert.crt+rootCA.privkey in this directory)
2.) Only once!: Install rootCA certificate in Windows
  - right click on file rootCA.cert.crt
  - choose "Install Certificate"
  - choose Local Computer
  - Select "Alle Zertifikate in folgendem Speicher speichern"
  - Click "Durchsuchen"
  - Select "Vertrauenswürdige Stammzertifizierungsstellen"
3.) Edit settings above (hostname is relevant for correct creation of host certificate)
4.) Edit usersettings/usersettings.ts
5.) Call gulp (among various other things, the rootCA certificate and its private key are read in and used to create/sign a host certificate)
6.) Build esp-idf project

*/

const DEFAULT_COUNTRY = 'Germany';
const DEFAULT_STATE = 'NRW';
const DEFAULT_LOCALITY = 'Greven';

const path = require("node:path");
import fs from "node:fs";
import { exec } from "node:child_process";
import * as os from 'node:os';

import * as gulp from "gulp";
import * as rollup from "rollup"
import rollupTypescript from '@rollup/plugin-typescript';
import * as brotli from 'brotli'
import terser from '@rollup/plugin-terser';
import nodeResolve from '@rollup/plugin-node-resolve';

import UserSettings from "../usersettings/usersettings";

import { inlineSource } from 'inline-source';
import * as sass from "sass";
import * as htmlMinifier from "html-minifier";
import * as cert from "./certificates"
import { getMac } from "./esp32";

const HTML_SPA_FILE = "app.html";
const HTML_SPA_FILE_BROTLI = HTML_SPA_FILE + ".br";
const SCSS_SPA_FILE = "app.scss";
const TS_MAIN_FILE = "app.ts";

const CONFIGURATION_TOOL_GENERATED_PATH = path.join("..", "..", "generated");

const ROOT = "..";
//Sources
const WEBUI_HTMLSCSS_PATH = path.join(ROOT, "webui_htmlscss");
const WEBUI_LOGIC_PATH = path.join(ROOT, "webui_logic");
const ROOT_CA_PEM_CRT = "rootCA.pem.crt";
const ROOT_CA_PEM_PRVTKEY = "rootCA.pem.prvtkey";
const HOST_CERT_PEM_CRT = "host.pem.crt";
const HOST_CERT_PEM_PRVTKEY = "host.pem.prvtkey";
const TESTSERVER_CERT_PEM_CRT = "testserver.pem.crt";
const TESTSERVER_CERT_PEM_PRVTKEY = "testserver.pem.prvtkey";
const TESTSERVER_PATH = path.join(ROOT, "testserver");

const WEBUI_TSCONFIG_PATH = path.join(WEBUI_LOGIC_PATH, "tsconfig.json");
const WEBUI_TYPESCRIPT_MAIN_FILE_PATH = path.join(WEBUI_LOGIC_PATH, TS_MAIN_FILE);
const FLATBUFFERS_SCHEMA_PATH = path.join(ROOT, "flatbuffers", "app.fbs");
const USERSETTINGS_PATH = path.join(ROOT, "usersettings", "usersettings.ts");

//intermediate and distribution
const GENERATED_PATH = path.join(ROOT, "generated");
const GENERATED_FLATBUFFER = path.join(GENERATED_PATH, "flatbuffers_gen");
const GENERATED_USERSETTINGS = path.join(GENERATED_PATH, "usersettings_gen");
const GENERATED_CERTIFICATES = path.join(ROOT, "certificates");
const DIST_WEBUI_PATH = path.join(ROOT, "dist_webui");
const DIST_WEBUI_RAW = path.join(DIST_WEBUI_PATH, "raw");
const DIST_WEBUI_BUNDELED = path.join(DIST_WEBUI_PATH, "bundeled");
const DIST_WEBUI_COMPRESSED = path.join(DIST_WEBUI_PATH, "compressed");
const DIST_FLATBUFFERS_CPP_HEADER = path.join(ROOT, "dist_cpp_header");
const DEST_FLATBUFFERS_TYPESCRIPT_WEBUI = path.join(WEBUI_LOGIC_PATH, "flatbuffers_gen");
const DEST_FLATBUFFERS_TYPESCRIPT_SERVER = path.join(TESTSERVER_PATH, "flatbuffers_gen");

const DEST_USERSETTINGS_PATH = path.join(WEBUI_LOGIC_PATH, "usersettings_copied_during_build.ts");

//Helpers

function writeFileCreateDirLazy(file: fs.PathOrFileDescriptor, data: string | NodeJS.ArrayBufferView, callback?: fs.NoParamCallback) {
  fs.mkdirSync(path.dirname(file), { recursive: true });
  if (callback) {
    fs.writeFile(file, data, callback);
  } else {
    fs.writeFileSync(file, data);
  }

}


function clean(cb: gulp.TaskFunctionCallback) {
  [DIST_WEBUI_PATH, DIST_FLATBUFFERS_CPP_HEADER, GENERATED_FLATBUFFER, DEST_FLATBUFFERS_TYPESCRIPT_SERVER, DEST_FLATBUFFERS_TYPESCRIPT_WEBUI, DEST_USERSETTINGS_PATH].forEach((path) => {
    fs.rmSync(path, { recursive: true, force: true });
  });
  cb();
}

//Code Generation
const theusersettings = UserSettings.Build();
function usersettings_generate_cpp_code(cb: gulp.TaskFunctionCallback) {
  console.log(`User settings has ${theusersettings.length} groups`);
  var codeBuilder: string = "//Header";
  theusersettings.forEach((cg, i, a) => {
    cg.items.forEach((ci, j, cia) => {
      codeBuilder = ci.RenderCPP(codeBuilder);
    });
  });
  writeFileCreateDirLazy(path.join(GENERATED_USERSETTINGS, "usersettings_accessor_gen.hh"), codeBuilder, cb);
}

//this is necessary to copy the usersettings in the context of the browser client project. There, the usersettings_base.ts is totally different from the one used in the build process
function usersettings_distribute_ts(cb: gulp.TaskFunctionCallback) {
  fs.copyFile(USERSETTINGS_PATH, DEST_USERSETTINGS_PATH, cb);
}

function flatbuffers_generate_c(cb: gulp.TaskFunctionCallback) {
  exec(`flatc -c --gen-all -o ${DIST_FLATBUFFERS_CPP_HEADER} ${FLATBUFFERS_SCHEMA_PATH}`, (err, stdout, stderr) => {
    cb(err);
  });
}

function flatbuffers_generate_ts(cb: gulp.TaskFunctionCallback) {
  exec(`flatc -T --gen-all --ts-no-import-ext -o ${GENERATED_FLATBUFFER} ${FLATBUFFERS_SCHEMA_PATH}`, (err, stdout, stderr) => {
    cb(err);
  });
}

function flatbuffers_distribute_ts(cb: gulp.TaskFunctionCallback) {
  fs.cpSync(GENERATED_FLATBUFFER, DEST_FLATBUFFERS_TYPESCRIPT_WEBUI, { recursive: true });
  fs.cpSync(GENERATED_FLATBUFFER, DEST_FLATBUFFERS_TYPESCRIPT_SERVER, { recursive: true });
  cb();
}



function typescriptCompile(cb: gulp.TaskFunctionCallback) {
  return rollup
    .rollup({
      input: WEBUI_TYPESCRIPT_MAIN_FILE_PATH,
      plugins: [
        rollupTypescript({ tsconfig: WEBUI_TSCONFIG_PATH }),
        nodeResolve()
      ]
    })
    .then(bundle => {
      return bundle.write({
        file: DIST_WEBUI_RAW + '/app.js',
        format: 'iife',
        name: 'MyApp',
        sourcemap: debug ? "inline" : false,
        plugins: [terser()]
      });
    });
}

function scssTranspile(cb: gulp.TaskFunctionCallback) {
  var result = sass.compile(path.join(WEBUI_HTMLSCSS_PATH, SCSS_SPA_FILE), { style: "compressed" });
  writeFileCreateDirLazy(path.join(DIST_WEBUI_RAW, "app.css"), result.css, cb);
}

const htmlMinifyOptions = {
  includeAutoGeneratedTags: true,
  removeAttributeQuotes: true,
  removeComments: true,
  removeRedundantAttributes: true,
  removeScriptTypeAttributes: true,
  removeStyleLinkTypeAttributes: true,
  sortClassName: true,
  useShortDoctype: true,
  collapseWhitespace: true
};

function htmlInline(cb: any) {
  inlineSource(path.join(WEBUI_HTMLSCSS_PATH, HTML_SPA_FILE), {
    compress: false,//needs to be false, as (2023-11-17), I get an SyntaxError: Unexpected token: punc (.)
    rootpath: DIST_WEBUI_RAW
  }).then((html) => { writeFileCreateDirLazy(path.join(DIST_WEBUI_BUNDELED, HTML_SPA_FILE), html, cb) });
}
function htmlMinify(cb: any) {
  let html = htmlMinifier.minify(fs.readFileSync(path.join(DIST_WEBUI_BUNDELED, HTML_SPA_FILE)).toString(), htmlMinifyOptions);
  writeFileCreateDirLazy(path.join(DIST_WEBUI_COMPRESSED, HTML_SPA_FILE), html, cb);
}

function htmlBrotli(cb: any) {
  let compressed = brotli.compress(fs.readFileSync(path.join(DIST_WEBUI_COMPRESSED, HTML_SPA_FILE)));
  writeFileCreateDirLazy(path.join(DIST_WEBUI_COMPRESSED, HTML_SPA_FILE_BROTLI), compressed, cb);
}

exports.build = gulp.series(
  clean,
  usersettings_generate_cpp_code,
  usersettings_distribute_ts,
  flatbuffers_generate_c,
  flatbuffers_generate_ts,
  flatbuffers_distribute_ts,
  scssTranspile,
  typescriptCompile,
  htmlInline,
  htmlMinify,
  htmlBrotli
);

exports.clean = clean;
exports.rootCA = (cb: gulp.TaskFunctionCallback) => {
  let CA = cert.CreateRootCA();
  writeFileCreateDirLazy(path.join(GENERATED_CERTIFICATES, ROOT_CA_PEM_CRT), CA.certificate);
  writeFileCreateDirLazy(path.join(GENERATED_CERTIFICATES, ROOT_CA_PEM_PRVTKEY), CA.privateKey, cb);
}

exports.certificates = (cb: gulp.TaskFunctionCallback) => {
  const hostname = fs.readFileSync("hostname.txt").toString();//esp32host_2df5c8
  const this_pc_name = os.hostname();
  let caCertPath = path.join(GENERATED_CERTIFICATES, ROOT_CA_PEM_CRT);
  let caPrivkeyPath = path.join(GENERATED_CERTIFICATES, ROOT_CA_PEM_PRVTKEY);
  let hostCert = cert.CreateCert(hostname, caCertPath, caPrivkeyPath);
  writeFileCreateDirLazy(path.join(GENERATED_CERTIFICATES, HOST_CERT_PEM_CRT), hostCert.certificate);
  writeFileCreateDirLazy(path.join(GENERATED_CERTIFICATES, HOST_CERT_PEM_PRVTKEY), hostCert.privateKey, cb);
  let testserverCert = cert.CreateCert(this_pc_name, caCertPath, caPrivkeyPath);
  writeFileCreateDirLazy(path.join(GENERATED_CERTIFICATES, TESTSERVER_CERT_PEM_CRT), testserverCert.certificate);
  writeFileCreateDirLazy(path.join(GENERATED_CERTIFICATES, TESTSERVER_CERT_PEM_PRVTKEY), testserverCert.privateKey, cb);
}

exports.getmac = async (cb:gulp.TaskFunctionCallback)=>{
  return getMac(comPort, hostnamePrefix);
}

exports.default = exports.build
