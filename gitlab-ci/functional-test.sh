#!/bin/bash
set -e
echo "preparation script"

pwd

supervisorctl status all
supervisorctl update all
supervisorctl restart all
sleep 3
supervisorctl status all

make functional-test -C ./build

#report part
#GitLab-CI output
#make functional-coverage -C ./build

supervisorctl stop all

gcovr -r . --xml -o ./coverage.xml
gcovr -r .
