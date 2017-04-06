#!/bin/bash
#
# Daemon Name: myDaemon

# Source function library.
. /lib/lsb/init-functions

prog=server
progLocation=./server/server

start() {
    # Start our daemon daemon
    echo -n $"Starting $prog"
    $progLocation
    RETVAL=$?
    echo
    return $RETVAL
}

stop() {
    echo -n $"Shutting down $prog"
    killall $prog
    RETVAL=$?
    echo
    return $RETVAL
}

# See how we were called.
case "$1" in
  start)
        start
        ;;
  stop)
        stop
        ;;
  status)
        status $prog
        ;;
  restart)
        stop
        start
        ;;
   *)
        echo $"Usage: $0 {start|stop|status|restart}"
        exit 2
esac