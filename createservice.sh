#!/bin/bash

RUNNABLE=
BASE=

trimTailingSpaces(){
    test -f $1 && sed -i 's/\s\+$//g' $1
}

checkRunnable(){
    [ $1 ] && [ -f $1 ] || return 1
    chmod +x $1
    RUNNABLE=`realpath $1`
    BASE=`basename $RUNNABLE`
}

createService(){
    checkRunnable $1
    shift
    cat>$BASE.service <<EOF
[Unit]
Description=$BASE
After=network.target

[Service]
Type=simple
User=$USER
Environment="ENABLE_LOGGER=1"

PIDFile=/tmp/$BASE.pid
ExecStart=$RUNNABLE $*

[Install]
WantedBy=multi-user.target
EOF
    trimTailingSpaces $BASE.service
}
createService $*