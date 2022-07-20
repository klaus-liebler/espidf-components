var gulp = require("gulp");

var source = require("vinyl-source-stream");
var ts = require("gulp-typescript");
var tsProject = ts.createProject("tsconfig.json");
const minify = require('gulp-minify');
var sourcemaps = require("gulp-sourcemaps");
var buffer = require("vinyl-buffer");
const { series, parallel } = require("gulp");
const sass = require('gulp-sass')(require('sass'));
const gulp_clean = require('gulp-clean');
const gzip = require('gulp-gzip');
const inlinesource = require('gulp-inline-source');

var paths = {
    pages: ["src/*.html"],
  };
  
function clean(cb)
{
    return gulp.src('{dist,dist_compressed}', {read: false, allowEmpty:true}).pipe(gulp_clean());
}


function tsTranspileAndBundleAndMinify(cb)
{
    return tsProject.src()
      .pipe(tsProject()).js
      .pipe(minify())
      .pipe(sourcemaps.write("./"))
      .pipe(gulp.dest("./"));
}

function cssTranspile(cb)
{
    return gulp.src('./src/app.scss')
    .pipe(sourcemaps.init())
    .pipe(sass({outputStyle: 'compressed'}).on('error', sass.logError))
    .pipe(sourcemaps.write('./'))
    .pipe(gulp.dest('./dist'));
}



function htmlCopy(cb)
{
    return gulp.src(paths.pages).pipe(gulp.dest("dist"));
}

function htmlInline(cb)
{
    return gulp.src('./dist/wifimanager.html')
    .pipe(inlinesource())
    .pipe(gzip())
    .pipe(gulp.dest('./dist_compressed'));
}



exports.build =  series(
    clean,
    cssTranspile,
    tsTranspileAndBundleAndMinify,
    htmlCopy,
    htmlInline
  );
exports.default = exports.build;
exports.cssTranspile=cssTranspile;
