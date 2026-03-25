#!/bin/bash

# The name of the following file may change. Find the correct one in source:
# https://github.com/IstvanBondar/iLoc
# The download source corresponds to the iloc documentation
URL="https://github.com/IstvanBondar/SeisComp-iLoc-plugin/archive/refs/tags/sciLoc4.3.tar.gz"

error() {
	echo $1
	exit 1
}

echo "Installing iLoc dependencies"

echo -n "User name for owning the SeisComP installation [${USER}]: "
read userName

if [ -z "$userName" ]; then
	userName=$USER
fi

seiscompTemp=/home/${userName}/seiscomp

echo -n "SeisComP installation directory [${seiscompTemp}]: "
read seiscomp

if [ -z "$seiscomp" ]; then
	seiscomp=${seiscompTemp}
fi

if [ ! -d "${seiscomp}" ]; then
	error "Directory '${seiscomp}' does not exist"
fi

iloc=${seiscomp}/share/iloc

mkdir -p "${iloc}" || error "Could not create target path '${iloc}'"

tarFile="$(mktemp)"

echo "Downloading ${URL} to ${tarFile}"
wget -O "${tarFile}" "${URL}" || error "Failed to download '${URL}' to '${tarFile}'"

cd ${iloc}
echo "Extracting TAR ball to '${iloc}'"
tar zxf ${tarFile} SeisComp-iLoc-plugin-sciLoc4.3/iLocAuxDir --strip-components=1 -C ${iloc} || error "Failed to extract '${tarFile}'"
echo "Changing ownership of '${iloc}' to '$userName' - you may wish to set group ownership"
chown -R $userName ${iloc}

exit 0
