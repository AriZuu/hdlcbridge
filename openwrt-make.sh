PATH=$HOME/openwrt/staging_dir/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin:$PATH
STAGING_DIR=$HOME/openwrt/staging_dir/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2
export PATH
export STAGING_DIR

CC=mipsel-openwrt-linux-gcc
export CC

make $*

