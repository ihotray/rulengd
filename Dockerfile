FROM ubuntu:16.04

LABEL maintainer="jakob.olsson@iopsys.eu"
LABEL build="docker build -t iopsys/rulengd ."
LABEL run="docker run -d --name rulengd --privileged --rm -v ${PWD}:/opt/work -p 2222:22 -e LOCAL_USER_ID=`id -u $USER` iopsys/rulengd:latest"
LABEL exec="docker exec --user=user -it rulengd bash"
LABEL stop="docker stop rulengd"

RUN \
      apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
      # general tools
      git \
      cmake \
      wget \
      build-essential \
      lcov \
      apt-utils \
      autoconf \
      automake \
      pkg-config \
      libtool \
      vim \
      valgrind \
      gdb \
      cppcheck \
      python3 \
      python3-setuptools \
      openssh-server \
      clang-format \
      sudo \
      strace \
      supervisor \
      net-tools \
      iputils-ping

# Configure OpenSSH server
RUN mkdir /var/run/sshd
RUN echo 'root:root' | chpasswd
RUN sed -i 's/PermitRootLogin prohibit-password/PermitRootLogin yes/' /etc/ssh/sshd_config
RUN sed -i 's/PermitEmptyPasswords no/PermitEmptyPasswords yes/' /etc/ssh/sshd_config
RUN sed 's@session\s*required\s*pam_loginuid.so@session optional pam_loginuid.so@g' -i /etc/pam.d/sshd

# Configure gdb-dashboard
RUN \
      easy_install3 pip && \
      pip3 install Pygments
RUN wget -P ~ git.io/.gdbinit
RUN \
      mkdir ~/.gdbinit.d && \
      touch ~/.gdbinit.d/init && \
      echo "dashboard -layout source" >> ~/.gdbinit.d/init && \
      echo "dashboard source -style context 20" >> ~/.gdbinit.d/init && \
      echo "dashboard -style syntax_highlighting 'monokai'" >> ~/.gdbinit.d/init

# Install dependent libraries
RUN \
      apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
      lua5.1-dev \
      lua5.1 \
      #libjson0 \
      #libjson0-dev \
      libssl-dev \
      libuv1-dev \
      cmocka-doc \
      libcmocka-dev \
      libcmocka0


# Install CMocka
#RUN \
#      git clone --branch cmocka-1.1.1 git://git.cryptomilk.org/projects/cmocka.git && \
#      cd cmocka && \
#      mkdir build && \
#      cd build && \
#      cmake .. && \
#      make && \
#      make install

# Remove cached packages.
RUN rm -rf /var/lib/apt/lists/*

RUN mkdir /opt/dev

# Install JSON-C
RUN \
      cd /opt/dev && \
      git clone https://github.com/json-c/json-c.git && \
      cd json-c && \
      sh autogen.sh && \
      ./configure && \
      make && \
      make install && \
      sudo ldconfig

# ubox
RUN \
      cd /opt/dev && \
      git clone  git://git.openwrt.org/project/libubox.git && \
      cd libubox && mkdir build && cd build && \
      git checkout fd57eea9f37e447814afbf934db626288aac23c4 && \
      cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE:String="Release" .. && \
      make -j2 && \
      make install

# uci
RUN \
      cd /opt/dev && \
      git clone git://nbd.name/uci.git && \
      cd uci && \
      git checkout a536e300370cc3ad7b14b052b9ee943e6149ba4d && \
      cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE:String="Release" -DBUILD_LUA=OFF . && \
      make -j2 && \
      make install

# ubus
RUN \
      cd /opt/dev && \
      git clone https://git.openwrt.org/project/ubus.git && \
      cd ubus && \
      git checkout 221ce7e7ff1bd1a0c9995fa9d32f58e865f7207f && \
      cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE:String="Release" -DBUILD_LUA=OFF -DBUILD_EXAMPLES=OFF . && \
      make -j2 && \
      make install

# rpcd
RUN \
      cd /opt/dev && \
      git clone https://git.openwrt.org/project/rpcd.git && \
      cd rpcd && \
      git checkout cfe1e75c91bc1bac82e6caab3e652b0ebee59524 && \
      cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE:String="Release" -DIWINFO_SUPPORT=NO . && \
      make -j2 && \
      make install && \
      mkdir /usr/lib/rpcd && \
      cp file.so /usr/lib/rpcd

# json-editor
RUN \
      cd /opt/dev && \
      git clone https://dev.iopsys.eu/iopsys/json-editor.git && \
      cd json-editor && \
      git checkout 44b32937a062ec4ffc9f7355841dc94ab6efa50f && \
      cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE:String="Release" . && \
      make && \
      make install

RUN mkdir /etc/config

WORKDIR /opt/work

# Expose ports
EXPOSE 22

# Prepare supervisor
RUN mkdir -p /var/log/supervisor
COPY supervisord.conf /etc/supervisor/conf.d/supervisord.conf

# Start entrypoint
COPY entrypoint.sh /usr/local/bin/entrypoint.sh
ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
