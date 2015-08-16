#!/bin/bash

cd /tmp

wget -rc -nd "http://kernel.ubuntu.com/~kernel-ppa/mainline/v4.1.5-unstable/linux-headers-4.1.5-040105-generic_4.1.5-040105.201508101730_amd64.deb"
wget -rc -nd "http://kernel.ubuntu.com/~kernel-ppa/mainline/v4.1.5-unstable/linux-headers-4.1.5-040105_4.1.5-040105.201508101730_all.deb"
wget -rc -nd "http://kernel.ubuntu.com/~kernel-ppa/mainline/v4.1.5-unstable/linux-image-4.1.5-040105-generic_4.1.5-040105.201508101730_amd64.deb"

sudo aptitude update -y
sudo aptitude install debconf-utils

sudo debconf-set-selections <<< 'grub grub/update_grub_changeprompt_threeway  select  install_new'
sudo debconf-set-selections <<< 'grub-legacy-ec2 grub/update_grub_changeprompt_threeway  select install_new'

dpkg -i linux-headers*deb linux-image*deb

cp /usr/src/linux-headers-4.1.5-040105/include/uapi/linux/tcp.h /usr/include/linux/tcp.h
sed -i 's/_UAPI//g' /usr/include/linux/tcp.h
