#!/bin/sh -e 

BASE="$(realpath $(dirname ${0}))"

"${BASE}"/build.sh && "${BASE}"/upload.sh $@
