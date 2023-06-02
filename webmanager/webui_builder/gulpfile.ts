import  * as gulp from "gulp"
import * as ts from "gulp-typescript";
import path from "node:path";
const gulp_clean = require("gulp-clean");
const minify =require("gulp-minify");
const sourcemaps= require("gulp-sourcemaps");
const gzip = require("gulp-gzip");
const inlinesource = require("gulp-inline-source");
const rollup = require("gulp-rollup");
const sass = require('gulp-sass')(require('sass'));
const htmlmin = require("gulp-htmlmin");

const html_spa_file = "webmanager.html";
const scss_spa_file = "app.scss";
const tsconfig_path = path.join("..", "webui_typescript", "tsconfig.json");

const htmlscss_base_path= "../webui_htmlscss";
const dist_base_path = "./dist";
const dist_bundeled_base_path="./dist_bundeled";
const dist_compressed_base_path="./dist_compressed";
const app_basename = "app";


var tsProject = ts.createProject(tsconfig_path);

function clean(cb:any)
{
  return gulp.src([dist_base_path, dist_bundeled_base_path, dist_compressed_base_path], {read: false, allowEmpty:true}).pipe(gulp_clean());
}


function tsTranspile(cb:any)
{
  return tsProject.src().pipe(tsProject()).js.pipe(gulp.dest(dist_base_path));
}

function jsBundle(cb:any){
  return gulp.src(dist_base_path+"/**/*.js")
  .pipe(sourcemaps.init())
    // transform the files here.
    .pipe(rollup({
      input: path.join(dist_base_path, app_basename)+".js",
      output:{
        format:"esm"
      }
    }))
  .pipe(sourcemaps.write())
  .pipe(gulp.dest(dist_bundeled_base_path));
}

function scssTranspile(cb:any)
{
    return gulp.src(htmlscss_base_path+"/"+scss_spa_file)
    .pipe(sourcemaps.init())
    .pipe(sass({outputStyle: 'compressed'}).on('error', sass.logError))
    .pipe(sourcemaps.write('./'))
    .pipe(gulp.dest(dist_bundeled_base_path));
}



function htmlMinify(cb:any)
{
    return gulp.src(htmlscss_base_path+"/*.html").pipe(htmlmin({ collapseWhitespace: true }))
    .pipe(gulp.dest(dist_bundeled_base_path));
}

function htmlInline(cb:any)
{
    return gulp.src(dist_bundeled_base_path+"/*.html")
    .pipe(inlinesource())
    .pipe(gulp.dest(dist_compressed_base_path));
}

function htmlGzip(cb:any)
{
    return gulp.src(dist_compressed_base_path+"/"+html_spa_file)
    .pipe(gzip())
    .pipe(gulp.dest(dist_compressed_base_path));
}

exports.build =  gulp.series(
  clean,
  scssTranspile,
  tsTranspile,
  jsBundle,
  htmlMinify,
  htmlInline,
  htmlGzip
);
exports.default = exports.build