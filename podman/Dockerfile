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
RUN dnf install -y meson gcc g++ libyaml libyaml-devel check check-devel mongo-c-driver mongo-c-driver-devel mongo-c-driver-libs which mongodb-org bc
