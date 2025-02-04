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
	OUTDIR=$(realpath "$1")
	echo "Using passed directory ${OUTDIR} for output"
fi

# Create OUTDIR if it doesn't exist
if ! mkdir -p "${OUTDIR}"; then
    echo "ERROR: Failed to create output directory ${OUTDIR}"
    exit 1
fi

echo "Output directory is set to ${OUTDIR}"

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    # Clean previous build artifacts
    #make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper

    # Configure the kernel with default settings for ARM64
    #make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig

    # Compile the kernel - this step takes time
    #make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all

    make        ARCH=arm64    CROSS_COMPILE=${CROSS_COMPILE} defconfig
    make -j4    ARCH=arm64    CROSS_COMPILE=${CROSS_COMPILE} all
    make        ARCH=arm64    CROSS_COMPILE=${CROSS_COMPILE} modules
    make        ARCH=arm64    CROSS_COMPILE=${CROSS_COMPILE} dtbs

    # Ensure the kernel image exists
    if [ ! -e arch/${ARCH}/boot/Image ]; then
        echo "ERROR: Kernel build failed. Image not found!"
        exit 1
    fi

    echo "Kernel build complete!"

    # Copy the built kernel image to OUTDIR for easy access
    cp arch/${ARCH}/boot/Image ${OUTDIR}/
fi



echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir -p ${OUTDIR}/rootfs/{bin,etc,proc,sys,dev,home,lib,lib64,sbin,tmp,usr,var}
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log


cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
    make defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
make                                ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} install

cd ${OUTDIR}/rootfs

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
export SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)

cp -L ${SYSROOT}/lib/ld-linux-aarch64.*     lib/
cp -L ${SYSROOT}/lib64/libm.so.*            lib64/
cp -L ${SYSROOT}/lib64/libresolv.so.*       lib64/
cp -L ${SYSROOT}/lib64/libc.so.*            lib64/


# TODO: Make device nodes
echo "Creating device nodes..."
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3
sudo mknod -m 600 ${OUTDIR}/rootfs/dev/console c 5 1


# TODO: Clean and build the writer utility
echo "Building writer utility..."
cd ${FINDER_APP_DIR}
make clean
make CROSS_COMPILE=${CROSS_COMPILE}
cp writer ${OUTDIR}/rootfs/home/


# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp ${FINDER_APP_DIR}/finder-test.sh       ${OUTDIR}/rootfs/home
cp ${FINDER_APP_DIR}/conf/            -r  ${OUTDIR}/rootfs/home
cp ${FINDER_APP_DIR}/finder.sh            ${OUTDIR}/rootfs/home
cp ${FINDER_APP_DIR}/writer               ${OUTDIR}/rootfs/home
cp ${FINDER_APP_DIR}/autorun-qemu.sh      ${OUTDIR}/rootfs/home

#Modify the finder-test.sh script to reference conf/assignment.txt instead of ../conf/assignment.txt.
sed -i 's|\.\./conf/assignment.txt|/home/conf/assignment.txt|' ${OUTDIR}/rootfs/home/finder-test.sh


# TODO: Chown the root directory
echo "Setting ownership to root for entire rootfs..."
sudo chown -R root:root ${OUTDIR}/rootfs


# TODO: Create initramfs.cpio.gz
echo "Creating initramfs..."
cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd ${OUTDIR}
gzip -f initramfs.cpio

if [ ! -f "${OUTDIR}/Image" ]; then
    echo "ERROR: Kernel Image not found in ${OUTDIR}!"
    exit 1
fi

if [ ! -f "${OUTDIR}/initramfs.cpio.gz" ]; then
    echo "ERROR: Initramfs not created successfully!"
    exit 1
fi

echo "Adding the Image in outdir"
cp /tmp/aeld/linux-stable/arch/arm64/boot/Image ${OUTDIR}/