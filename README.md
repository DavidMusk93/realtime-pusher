# realtime-pusher
Collects data from multicast clients, encode &amp; push it to tcp server, then decode &amp; pack &amp; push it to websocket clients.

## 1. Prerequisites

On your bash shell, simplily execute `bash prerequisites.sh`. This would download the latest websocketpp package from its github [release page](https://github.com/zaphoyd/websocketpp/releases) and install `libboost-all-dev`(required by websocketpp), `libmysqlcppconn-dev`(to save raw data to mysql).

## 2. Compile

Goto realtime-pusher folder, run

```
mkdir -p build
cd build
cmake ..
make
```

then, you would get two executable `collector` & `notifier`.

## 3. Run

### 3.1 Launch directly

In this mode, log would output to `stdout`.

For COLLECTOR(run on local side), `collector 185,186` followed by the topic names. There three macros should be cared (in `collector/main.cpp`):

* GROUP_IP, multicast address;
* SERVER_IP, websocket server address;
* SERVER_PORT, wesocket server port for tcp.

For NOTIFIER(run on server side), `notifier`. The default websocket port is 9002 (could be found in `notifier/main.cpp`) and tcp port is 10001 (a static variable `RemoteReader::PORT` in `notifier/notifier.cpp`). 

### 3.2 Run as systemd service

Generate `collector.service` (bash createservice.sh ~/collector):

```
[Unit]
Description=collector
After=network.target

[Service]
Type=simple
User=sun
Environment="ENABLE_LOGGER=1"

PIDFile=/tmp/collector.pid
ExecStart=/home/sun/collector 185,186

[Install]
WantedBy=multi-user.target
```

Generate `notifier.service` (bash createservice.sh ~/notifier 185,186):

```
[Unit]
Description=notifier
After=network.target

[Service]
Type=simple
User=sun
Environment="ENABLE_LOGGER=1"

PIDFile=/tmp/notifier.pid
ExecStart=/home/sun/notifier

[Install]
WantedBy=multi-user.target
```

For both services, log will redirect to file (corresponding to /tmp/collector.log & /tmp/notifier.log). To enable this, just set environment variable `ENABLE_LOGGER`.

### 3.3 Database

For COLLECTOR, to save raw data to database, just set enviroment variable `ENABLE_DB`. The table is as follows:

```
use datacenter;

CREATE TABLE `record`
(
    `id`        int(12) PRIMARY KEY NOT NULL AUTO_INCREMENT,
    `timestamp` datetime DEFAULT CURRENT_TIMESTAMP COMMENT 'create timestamp',
    `ip`        varchar(16)         NOT NULL,
    `data`      varchar(64)         NOT NULL
) ENGINE=InnoDB CHARSET=utf8;
```
For more details, see `collector/connector.h`.

### 4. Design

![diagram](https://github.com/DavidMusk93/realtime-pusher/blob/master/diagram.svg)
