#!/bin/bash

echo "preparation script"

pwd

supervisorctl status all
supervisorctl update all
sleep 3
supervisorctl status all

make functional-test -C ./build

#report part
#GitLab-CI output
make functional-coverage -C ./build CTEST_OUTPUT_ON_FAILURE=TRUE

supervisorctl stop all
supervisorctl status

