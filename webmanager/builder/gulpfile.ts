const debug=true;

import * as gulp from "gulp"
const path =require("node:path");
import * as child_process from "node:child_process";
import * as rollup from "rollup"
import rollupTypescript from '@rollup/plugin-typescript';
//const tsify = require('tsify');
//const browserify = require('browserify');
//const source = require('vinyl-source-stream');
//const buffer = require('vinyl-buffer');

import * as ts from "gulp-typescript";


const gulp_clean = require("gulp-clean");
const gzip = require("gulp-gzip");
const inlinesource = require("gulp-inline-source");
const sass = require('gulp-sass')(require('sass'));
const htmlmin = require("gulp-htmlmin");

const HTML_SPA_FILE = "app.html";
const SCSS_SPA_FILE = "app.scss";
const TS_MAIN_FILE = "app.ts";

const CONFIGURATION_TOOL_GENERATED_PATH=path.join("..", "..", "generated");

const WEBUI_HTMLSCSS_PATH = path.join("..", "webui_htmlscss");
const WEBUI_LOGIC_PATH = path.join("..", "webui_logic");
const DIST_WEBUI_PATH = path.join("..", "dist_webui");
const TSCONFIG_PATH = path.join(WEBUI_LOGIC_PATH, "tsconfig.json");
const TYPESCRIPT_MAIN_FILE_PATH = path.join(WEBUI_LOGIC_PATH, "src", TS_MAIN_FILE );
const DIST_WEBUI_RAW =path.join(DIST_WEBUI_PATH, "raw");
const DIST_WEBUI_BUNDELED =path.join(DIST_WEBUI_PATH, "bundeled");
const DIST_WEBUI_COMPRESSED =path.join(DIST_WEBUI_PATH, "compressed");
const DIST_FLATBUFFERS_CPP_HEADER = path.join("..", "dist_cpp_header");
const DIST_FLATBUFFERS_TYPESCRIPT = path.join(WEBUI_LOGIC_PATH,"src", "generated");
const FLATBUFFERS_SCHEMA_PATH= path.join("..", "flatbuffers", "app.fbs");

const app_basename = "app";


var tsProject = ts.createProject(TSCONFIG_PATH);

function clean(cb:any)
{
  return gulp.src([DIST_WEBUI_PATH, DIST_FLATBUFFERS_CPP_HEADER, DIST_FLATBUFFERS_TYPESCRIPT], {read: false, allowEmpty:true}).pipe(gulp_clean({force:true}));
}

function flatbuffers_c(cb: gulp.TaskFunctionCallback){
  child_process.exec(`flatc -c --gen-all -o ${DIST_FLATBUFFERS_CPP_HEADER} ${FLATBUFFERS_SCHEMA_PATH}`, (err, stdout, stderr) =>{
    cb(err);
  });
}

function flatbuffers_ts(cb: gulp.TaskFunctionCallback){
  child_process.exec(`flatc -T --gen-all --ts-no-import-ext -o ${DIST_FLATBUFFERS_TYPESCRIPT} ${FLATBUFFERS_SCHEMA_PATH}`, (err, stdout, stderr) =>{
    cb(err);
  });
}
/*
function typescriptCompileBrowserify(cb: gulp.TaskFunctionCallback) {
  return browserify({
    standalone: "MyApp",
    debug: debug,
  })
    .add(TYPESCRIPT_MAIN_FILE_PATH)

    .plugin(tsify, {
      noImplicitAny: true,
      project: TSCONFIG_PATH,
    })
    .bundle()

    .pipe(source('app.js'))
    .pipe(buffer())
    .pipe(gulp.dest(DIST_WEBUI_RAW));
}*/

function typescriptCompile(cb: gulp.TaskFunctionCallback) {
  return rollup
		.rollup({
			input: TYPESCRIPT_MAIN_FILE_PATH,
			plugins: [rollupTypescript({tsconfig:"../webui_logic/tsconfig.json"})]
		})
		.then(bundle => {
			return bundle.write({
				file: DIST_WEBUI_RAW+'/app.js',
				format: 'iife',
				name: 'MyApp',
				sourcemap: debug?"inline":false,
			});
		});
}

function scssTranspile(cb:any)
{
    return gulp.src("../webui_htmlscss/*.scss")
    .pipe(sass({outputStyle: 'compressed'}).on('error', sass.logError))
    .pipe(gulp.dest("../dist_webui/raw/"));
}

function htmlMinify(cb:any)
{
    return gulp.src("../webui_htmlscss/*.html").pipe(htmlmin({ collapseWhitespace: true }))
    .pipe(gulp.dest("../dist_webui/raw/"));
}

function htmlInline(cb:any)
{
    return gulp.src("../dist_webui/raw/*.html")
    .pipe(inlinesource({ compress: false }))
    .pipe(gulp.dest("../dist_webui/bundeled/"));
}

function htmlGzip(cb:any)
{
    return gulp.src("../dist_webui/bundeled/*.html")
    .pipe(gzip())
    .pipe(gulp.dest("../dist_webui/compressed/"));
}

exports.build =  gulp.series(
  clean,
  flatbuffers_c,
  flatbuffers_ts,
  scssTranspile,
  typescriptCompile,
  htmlMinify,
  htmlInline,
  htmlGzip
);
exports.default = exports.build
exports.clean=clean;