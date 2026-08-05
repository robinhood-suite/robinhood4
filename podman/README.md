# Overview

- `mongodb.Dockerfile`, used to launch a MongoDB database for Robinhood.
- `robinhood.Dockerfile`, acts as an example with Robinhood installed and already configured to work connected to the MongoDB database. Built from source.

# Quickstart 

A Docker Compose setup is available to quickly start testing Robinhood4 and exploring its different commands.
First, launch the compose in `robinhood4/podman`:

```sh
pwd # .../robinhood4/podman
podman-compose up -d --build
podman ps -a # all services are running
```

This launches two services: 
- `mongodb`, which contains the databases (built from `mongodb.Dockerfile`)
- `robinhood`, which contains the application built from source (built from `robinhood.Dockerfile`).

Then you can access the containers using the following command :

```sh
podman exec -it robinhood bash
podman exec -it mongodb bash
```

## Configuration notes

- The `robinhood` container is preconfigured to access MongoDB automatically. The MongoDB host in the configuration file is changed from `localhost` to `mongodb` in `robinhood.Dockerfile` to match the `container_name` in the `docker-compose.yml`.
- A `test_dir` folder is created through `docker-compose` to reproduce the "First steps" example from the Wiki.
- The `:Z` volume option is used so that the folder is accessible only by the `robinhood` container.

# Example of a running docker-compose setup

```
──> podman exec -it robinhood bash 
[root@08106e189cf3 robinhood]# touch /test_dir/test_file
[root@08106e189cf3 robinhood]# rbh-sync rbh:posix:/test_dir rbh:mongo:test_database
[root@08106e189cf3 robinhood]# 
exit # Ctrl+D

──> podman exec -it mongodb bash 
[root@f8419a5c2051 app]# mongosh test_database --quiet --eval "db.entries.find()"
[
  {
    ...
    ns: [
      {
        parent: Binary.createFromBase64('AQCBAAAAz5sMAgAAAAAKQl5+', 0),
        name: 'test_file',
        xattrs: { path: '/test_file', sync_time: Long('1785851833') }
      }
    ]
  }
]

──> podman-compose down 
```
