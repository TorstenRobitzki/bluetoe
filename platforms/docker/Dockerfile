FROM frolvlad/alpine-glibc as base

RUN  apk update \
     && apk add --no-cache --update git tar wget ncurses libusb unzip ninja cmake make \
     && rm /lib64/ld-linux-x86-64.so.2 \
     && ln -s /usr/glibc-compat/lib/ld-linux-x86-64.so.2 /lib64/ld-linux-x86-64.so.2

RUN mkdir -p /opt/ && cd /opt &&  \
    wget -q https://www.nordicsemi.com/-/media/Software-and-other-downloads/SDKs/nRF5/Binaries/nRF5_SDK_17.1.0_ddde560.zip \
        && unzip -q nRF5_SDK_17.1.0_ddde560.zip "nRF5_SDK_17.1.0_ddde560/components/toolchain/cmsis/include/*" \
           "nRF5_SDK_17.1.0_ddde560/modules/*" -d . \
        && mv nRF5_SDK_17.1.0_ddde560 nrf5_sdk \
        && rm nRF5_SDK_17.1.0_ddde560.zip

RUN wget -q https://developer.arm.com/-/media/Files/downloads/gnu-rm/10.3-2021.10/gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2 -P /usr/local \
        && tar -C /usr/local -xf /usr/local/gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2 --no-same-owner \
        && rm /usr/local/gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2
