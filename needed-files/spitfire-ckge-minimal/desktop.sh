#!/bin/bash
echo final step needed for wallpaper and clean up
plasma-apply-wallpaperimage -f stretch /opt/Arch-Systemtool/systemtool-extras/SpitFire/spitfire.png > /dev/null 2>&1
sed -i '/^\[Desktop Entry\]/,/^\[/ s/^DefaultProfile=.*/DefaultProfile=claudemods-cyan.profile/' ~/.config/konsolerc
cd /home/$USER/claudemods-distribution-installer/installer && chmod +x cleanup.sh
rm -rf /home/$USER/.config/autostart/wallpaper.desktop
cd /home/$USER/claudemods-distribution-installer/installer && ./cleanup.sh
