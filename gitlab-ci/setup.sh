#!/bin/bash

echo "preparation script"

pwd

mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make
make install

#cp -r ./test/files/etc/* /etc/
#cp -r ./api/schemas/* /usr/share/rpcd/schemas
#cp ./gitlab-ci/iopsys-supervisord.conf /etc/supervisor/conf.d/
#
#ls /etc/config/
#ls /usr/share/rpcd/schemas/
#ls /etc/supervisor/conf.d/

