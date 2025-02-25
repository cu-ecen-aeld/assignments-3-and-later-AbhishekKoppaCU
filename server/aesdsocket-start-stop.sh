#!/bin/sh

COMMAND=$1
DaemonName="aesdsocket"
DaemonPath="/usr/bin/aesdsocket"

if [ -z "$COMMAND" ]; then
    echo "Usage: $0 start|stop"
    exit 1
fi

case "$COMMAND" in
    start)
        echo "Starting ${DaemonName}..."
        if start-stop-daemon -S -n ${DaemonName} -a ${DaemonPath} -- -d; then
            echo "${DaemonName} started successfully."
        else
            echo "Failed to launch ${DaemonName}."
            exit 1
        fi
        ;;

    stop)
        echo "Shutting down ${DaemonName}..."
        if start-stop-daemon -K -n ${DaemonName} --signal SIGTERM; then
            echo "${DaemonName} stopped."
        else
            echo "Failed to stop ${DaemonName}."
            exit 1
        fi
        ;;

    *)
        echo "Invalid command. Usage: $0 start|stop"
        exit 1
        ;;
esac

exit 0
