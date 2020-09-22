#!/bin/bash

downloadWebsocketppPackage(){
    download(){
        test -f $2 || curl -L $1 -o $2
    }
    OUT=./3rdparty
    PATTERN='[^"]+\.tar\.gz'
    URL_PREFIX=https://github.com
    RELEASE_PAGE=$URL_PREFIX/zaphoyd/websocketpp/releases/tag/0.8.2
    HTML=/tmp/`basename $RELEASE_PAGE`.html
    download $RELEASE_PAGE $HTML
    URL=$URL_PREFIX`egrep -o $PATTERN $HTML| head -n1`
    mkdir -p $OUT
    FILE=`basename $URL`
    [ $FILE ] || return 1
    download $URL $OUT/$FILE
    (cd $OUT && tar -zxvf $FILE && ln -sfn websocketpp-${FILE%.tar.gz} websocketpp)
}

installDependencies(){
    isInstalled(){
        dpkg-query -l| egrep -q 'ii[[:blank:]]*'$1'[[:blank:]]'
    }
    for pkg;do isInstalled $pkg || sudo apt install $pkg -y;done
}

main(){
    downloadWebsocketppPackage
    installDependencies libboost-all-dev libmysqlcppconn-dev
}
main