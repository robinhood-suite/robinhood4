# Use Python as base for Podman Build from Dockerfiles
FROM almalinux:latest

# Set the working directory in the container
WORKDIR /app

RUN dnf install -y 'dnf-command(config-manager)'

# Enable the CRB repository, which contains some dependencies
RUN dnf config-manager --set-enabled crb
RUN dnf install epel-release -y

RUN mkdir -p /data/db
COPY mongo_repo /etc/yum.repos.d/mongodb-org-8.0.repo

# Install any needed dependencies specified in requirements.txt
RUN dnf install -y meson gcc g++ libyaml libyaml-devel check check-devel mongo-c-driver mongo-c-driver-devel mongo-c-driver-libs which mongodb-org bc sudo

RUN chmod o+x /bin/bash

# Two notes:
#   - RobinHood 4 uses low-level system calls for some operations,
#     especially `name_to_handle_at`. This will not work in the container by
#     default. To make those calls work, you can run the container with the
#     mount option in bind mode, binding one of your node's directory to a
#     directory in the container. For instance, you can use the following:
#     `--mount=type=bind,source=/home/podman_stuff,destination=/app`
#     You will then have to export the following environement variable in your
#     container: `export TMPDIR="/app"`, so that all tests are run inside this
#     directory, and the commands are able to use the system calls.
#
#   - systemd is not available by default in the container, which means Mongo
#     cannot be used with systemctl. To solve this issue, simply start the
#     mongod process by hand with the following:
#     `mongod --fork --logpath /var/log/mongod.log`
