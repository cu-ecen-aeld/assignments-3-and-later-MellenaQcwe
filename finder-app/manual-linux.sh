#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

echo "Adding the Image in outdir"
cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    make ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} mrproper
    make ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} defconfig
    make -j4 ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} all
    make ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} modules
    make ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi
cp -rf ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}/

echo "Adding the busybox in outdir"
cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busyboxx
fi

echo "Adding the rootfs files hierachie in outdir"
cd "$OUTDIR"
if [ ! -d "${OUTDIR}/rootfs/bin" ]
then
    #echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    #sudo rm  -rf ${OUTDIR}/rootfs
    mkdir -p rootfs
    cd rootfs
    mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var 
    mkdir usr/bin usr/lib usr/sbin
    mkdir var/log
fi

echo "Compiling busybox and adding to rootfs"
cd "$OUTDIR"
if [ ! -e "${OUTDIR}/rootfs/bin/busybox" ]
then
    cd busybox
    make distclean
    make defconfig
    make ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE}
    make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} install
fi

echo "Adding busybox library dependencies to rootfs"
prog_interpreter_path=$(${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "program interpreter" | awk -F '[][]' '{print $2}' | awk '{print $4}')
prog_interpreter=$(basename ${prog_interpreter_path})
find / -name ${prog_interpreter} 2>/dev/null > $OUTDIR/temp.txt && cd "$OUTDIR"
prog_interpreter_src=$(cat ${OUTDIR}/temp.txt | grep $(echo ${CROSS_COMPILE} | awk '{sub(/.$/, ""); print}'))
echo "Copying ${prog_interpreter_src} to ${OUTDIR}/rootfs/lib/"
test -f ${OUTDIR}/rootfs/lib/${prog_interpreter} || cp ${prog_interpreter_src} ${OUTDIR}/rootfs/lib/
if [ ! -f "${OUTDIR}/rootfs/lib/${prog_interpreter}" ]
then 
    echo "Failed to copy ${prog_interpreter}" 
    exit 1
fi

libraries=($(${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "Shared library" | awk -F '[][]' '{print $2}'))
for i in "${!libraries[@]}"; do
    find / -name "${libraries[i]}" 2>/dev/null > $OUTDIR/temp.txt && cd "$OUTDIR"
    library_src=$(cat ${OUTDIR}/temp.txt | grep $(echo ${CROSS_COMPILE} | awk '{sub(/.$/, ""); print}'))
    echo "Copying ${library_src} to ${OUTDIR}/rootfs/lib64/"
    test -f ${OUTDIR}/rootfs/lib64/${library_src} || cp ${library_src} ${OUTDIR}/rootfs/lib64/
    if [ ! -f "${OUTDIR}/rootfs/lib64/${libraries[i]}" ] 
    then 
        echo "Failed to copy ${libraries[i]}" 
        exit 1
    fi
done

echo "Make device nodes for initramfs"
cd "$OUTDIR/rootfs"
stat -f dev/null > /dev/null 2>&1  || sudo mknod -m 666 dev/null c 1 3
stat -f dev/console > /dev/null 2>&1  || sudo mknod -m 666 dev/console c 5 1

echo "Clean and build the writer utility"
export CROSS_COMPILE=${CROSS_COMPILE}
cd "$FINDER_APP_DIR"
make clean
make

echo "Copying the finder related scripts and executables to the /home directory on the target rootfs"
cp -rf ${FINDER_APP_DIR}/finder-test.sh ${OUTDIR}/rootfs/home
cp -rf ${FINDER_APP_DIR}/finder.sh ${OUTDIR}/rootfs/home
cp -rf ${FINDER_APP_DIR}/writer ${OUTDIR}/rootfs/home
cp -rf ${FINDER_APP_DIR}/autorun-qemu.sh ${OUTDIR}/rootfs/home
cp -rf ${FINDER_APP_DIR}/conf/ ${OUTDIR}/rootfs/home

echo "Chown the root directory"
#???

echo "Create initramfs.cpio.gz"
cd "$OUTDIR/rootfs"
find . | cpio -H newc -ov --owner root:root > $OUTDIR/initramfs.cpio
cd "$OUTDIR"
gzip -f initramfs.cpio

