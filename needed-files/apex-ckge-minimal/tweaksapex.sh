#!/bin/bash

username="$1"

sudo -S chmod 4755 /usr/lib/spice-client-glib-usb-acl-helper
echo 'blacklist ntfs3' | sudo -S tee /etc/modprobe.d/disable-ntfs3.conf >/dev/null 2>&1
chsh -s $(which fish)
sudo -S chown -R $username:$username /home/$username 
sudo -S chown -R $username:$username /home/$username/*
