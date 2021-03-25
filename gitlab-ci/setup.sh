#!/bin/bash
set -e
echo "preparation script"

pwd

mkdir build
pushd build
	cmake .. -DCMAKE_BUILD_TYPE=Debug
	make
	make install
popd
#cp -r ./test/files/etc/* /etc/
#cp -r ./api/schemas/* /usr/share/rpcd/schemas
cp ./gitlab-ci/iopsys-supervisord.conf /etc/supervisor/conf.d/
#
#ls /etc/config/
#ls /usr/share/rpcd/schemas/
#ls /etc/supervisor/conf.d/

