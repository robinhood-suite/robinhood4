FROM almalinux:latest

WORKDIR /robinhood

# install any needed dependencies
RUN dnf install -y epel-release && \
    dnf install -y \
        gcc \
        gcc-c++ \
        make \
        meson \
        ninja-build \
        git \
        pkgconf-pkg-config \
        glib2-devel \
        libyaml-devel \
        libuuid-devel \
        python3 \
        python3-devel \
        python3-sphinx \
        openmpi-devel \
        mongo-c-driver-devel \
        jansson-devel \
    && \
    dnf clean all

# Installing miniyaml, required to build rb4
RUN git clone https://github.com/cea-hpc/miniyaml.git && \
    cd miniyaml && \
    meson builddir && \
    ninja -C builddir && \
    ninja -C builddir install


# for libminiyaml.so.1
ENV LD_LIBRARY_PATH=/usr/local/lib64:$LD_LIBRARY_PATH
# for libmpi.so
ENV LD_LIBRARY_PATH=/usr/lib64/openmpi/lib/:$LD_LIBRARY_PATH

# building robinhood4 from source
COPY . .
RUN meson setup --prefix="/usr" builddir
RUN ninja -C builddir
RUN ninja -C builddir install

# change the default localhost to work with the Dockerfile in the compose example
ARG MONGO_HOST=mongodb
RUN sed -i "s#mongodb://localhost:27017#mongodb://${MONGO_HOST}:27017#" /etc/robinhood4.d/default.yaml
