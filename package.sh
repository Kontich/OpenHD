#!/bin/bash

export LC_ALL=C.UTF-8
export LANG=C.UTF-8

PACKAGE_ARCH=$1
OS=$2
DISTRO=$3
BUILD_TYPE=$4



if [[ "${DISTRO}" == "buster" ]]; then
    PLATFORM_PACKAGES="-d wiringpi -d veye-raspberrypi -d lifepoweredpi -d raspi2png -d gstreamer1.0-omx-rpi-config -d gst-rpicamsrc -d qopenhd -d openhd-linux-pi -d libjpeg62-turbo"
    PLATFORM_CONFIGS="--config-files /boot/cmdline.txt --config-files /boot/config.txt --config-files /usr/local/share/openhd/joyconfig.txt"
fi

if [[ "${DISTRO}" == "bullseye" ]]; then
    PLATFORM_PACKAGES="-d veye-raspberrypi -d lifepoweredpi -d raspi2png -d gst-rpicamsrc -d qopenhd -d openhd-linux-pi -d libjpeg62-turbo"
    PLATFORM_CONFIGS="--config-files /boot/cmdline.txt --config-files /boot/config.txt --config-files /usr/local/share/openhd/joyconfig.txt"
fi

if [[ "${OS}" == "ubuntu" ]] && [[ "${PACKAGE_ARCH}" == "armhf" || "${PACKAGE_ARCH}" == "arm64" ]]; then
    echo "--------------ADDING nvidia-l4t-gstreamer to package list--------------- "
    PLATFORM_PACKAGES="-d nvidia-l4t-gstreamer -d qopenhd"
    PLATFORM_CONFIGS="--config-files /usr/local/share/openhd/joyconfig.txt"
fi

if [ "${BUILD_TYPE}" == "docker" ]; then
    cat << EOF > /etc/resolv.conf
options rotate
options timeout:1
nameserver 8.8.8.8
nameserver 8.8.4.4
EOF
fi


apt-get install -y apt-transport-https curl || exit 1

curl -1sLf 'https://dl.cloudsmith.io/public/openhd/openhd-2-1-testing/setup.deb.sh' | sudo -E bash && \
curl -1sLf 'https://dl.cloudsmith.io/public/openhd/openhd-2-1/setup.deb.sh' | sudo -E bash

apt -y update || exit 1

PACKAGE_NAME=openhd
PACKAGE_ARCH=armhf

PKGDIR=/tmp/${PACKAGE_NAME}-installdir

./install_dep.sh || exit 1

cd OpenHD

sudo rm -rf ${PACKAGE_DIR}/

rm -rf build

mkdir build

cd build

cmake ..
make -j4

ls -a

mkdir -p ${PACKAGE_DIR}/usr/local/bin || exit 1
cp OpenHD ${PACKAGE_DIR}/usr/local/bin/OpenHD || exit 1

VERSION="2.1-$(date '+%m%d%H')"

rm ${PACKAGE_NAME}_${VERSION//v}_${PACKAGE_ARCH}.deb > /dev/null 2>&1

fpm -a ${PACKAGE_ARCH} -s dir -t deb -n ${PACKAGE_NAME} -v ${VERSION//v} -C ${PKGDIR} \
  $PLATFORM_CONFIGS \
  -p ${PACKAGE_NAME}_VERSION_ARCH.deb \
  --after-install ../../after-install.sh \
  --before-install ../../before-install.sh \
  $PLATFORM_PACKAGES \
  -d "libasio-dev >= 1.10" \
  -d "libboost-system-dev >= 1.62.0" \
  -d "libboost-program-options-dev >= 1.62.0" \
  -d "libseek-thermal >= 20201118.1" \
  -d "flirone-driver >= 20200704.3" \
  -d "wifibroadcast >= 20200930.1" \
  -d "openhd-dump1090-mutability >= 20201122.2" \
  -d "gnuplot-nox" \
  -d "hostapd" \
  -d "iw" \
  -d "isc-dhcp-common" \
  -d "pump" \
  -d "dnsmasq" \
  -d "aircrack-ng" \
  -d "ser2net" \
  -d "i2c-tools" \
  -d "dos2unix" \
  -d "fuse" \
  -d "socat" \
  -d "ffmpeg" \
  -d "indent" \
  -d "libv4l-dev" \
  -d "libusb-1.0-0" \
  -d "libpcap-dev" \
  -d "libpng-dev" \
  -d "libnl-3-dev" \
  -d "libnl-genl-3-dev" \
  -d "libsdl2-2.0-0" \
  -d "libsdl1.2debian" \
  -d "libconfig++9v5" \
  -d "libreadline-dev" \
  -d "libjpeg-dev" \
  -d "libsodium-dev" \
  -d "libfontconfig1" \
  -d "libfreetype6" \
  -d "libgles2-mesa-dev" \
  -d "libboost-chrono-dev" \
  -d "libboost-regex-dev" \
  -d "libboost-filesystem-dev" \
  -d "libboost-thread-dev" \
  -d "gstreamer1.0-plugins-base" \
  -d "gstreamer1.0-plugins-good" \
  -d "gstreamer1.0-plugins-bad" \
  -d "gstreamer1.0-plugins-ugly" \
  -d "gstreamer1.0-libav" \
  -d "gstreamer1.0-tools" \
  -d "gstreamer1.0-alsa" \
  -d "gstreamer1.0-pulseaudio" || exit 1


  sudo rm -Rf 

#
# Only push to cloudsmith for tags. If you don't want something to be pushed to the repo, 
# don't create a tag. You can build packages and test them locally without tagging.
#
git describe --exact-match HEAD > /dev/null 2>&1
if [[ $? -eq 0 ]]; then
    echo "Pushing package to OpenHD repository"
    cloudsmith push deb openhd/openhd-2-1/${OS}/${DISTRO} ${PACKAGE_NAME}_${VERSION//v}_${PACKAGE_ARCH}.deb || exit 1
else
    echo "Pushing package to OpenHD testing repository"
    cloudsmith push deb openhd/openhd-2-1-testing/${OS}/${DISTRO} ${PACKAGE_NAME}_${VERSION//v}_${PACKAGE_ARCH}.deb || exit 1
fi
