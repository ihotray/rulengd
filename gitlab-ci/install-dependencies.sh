#!/bin/bash
echo "install dependencies"
pwd
# libeasy
cd /opt/dev
rm -fr easy-soc-libs
git clone https://dev.iopsys.eu/iopsys/easy-soc-libs.git
cd easy-soc-libs
git checkout devel
cd libeasy
git fetch --all
git pull origin devel
make clean
make CFLAGS+="-I/usr/include/libnl3"
mkdir -p /usr/include/easy
cp easy.h event.h utils.h if_utils.h debug.h /usr/include/easy
cp -a libeasy*.so* /usr/lib
sudo ldconfig
