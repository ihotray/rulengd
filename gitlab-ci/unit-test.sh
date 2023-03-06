#!/bin/bash
set -e
echo "preparation script"

pwd

make unit-test -C ./build


#report part
#GitLab-CI output
make unit-coverage -C ./build
