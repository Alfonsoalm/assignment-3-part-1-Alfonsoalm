
#!/bin/sh
### BEGIN INIT INFO
# Provides:          aesdsocket
# Required-Start:    $remote_fs $syslog $time
# Required-Stop:     $remote_fs $syslog $time
# Default-Start:     S
# Default-Stop:      0 6
# Short-Description: AESD socket server daemon
### END INIT INFO

DAEMON="/usr/bin/aesdsocket"
PIDFILE="/var/run/aesdsocket.pid"
NAME="aesdsocket"
DESC="AESD socket server"

# Verifica que el ejecutable exista
[ -x "$DAEMON" ] || { echo "$DAEMON no existe o no es ejecutable"; exit 1; }

ensure_dirs() {
    # /var/run es un tmpfs → se borra al arrancar
    [ -d /var/run ] || mkdir -p /var/run

    # /var/tmp debe existir para aesdsocketdata
    [ -d /var/tmp ] || mkdir -p /var/tmp
}

start() {
    ensure_dirs
    echo "Starting $DESC..."

    # -S  → start
    # -b  → background (daemoniza)
    # -m  → crea pidfile
    # -p  → ruta del pidfile
    start-stop-daemon -S -b -m -p "$PIDFILE" --exec "$DAEMON"
    RET=$?

    if [ $RET -eq 0 ]; then
        echo "$NAME started"
    else
        echo "Failed to start $NAME (error $RET)"
    fi
    return $RET
}

stop() {
    echo "Stopping $DESC..."

    if [ -f "$PIDFILE" ]; then
        # - K  → kill
        # Envia SIGTERM al proceso indicado en el pidfile
        start-stop-daemon -K -p "$PIDFILE" -s TERM

        # Espera a que se detenga limpiamente (máx 5 segundos)
        TIMEOUT=50
        while [ $TIMEOUT -gt 0 ] && [ -e "$PIDFILE" ]; do
            sleep 0.1
            TIMEOUT=$((TIMEOUT - 1))
        done

        # Elimina pidfile si aún existe
        [ -e "$PIDFILE" ] && rm -f "$PIDFILE"
    else
        # Fallback si no existe pidfile
        start-stop-daemon -K --exec "$DAEMON" -s TERM
    fi

    echo "$NAME stopped"
    return 0
}

status() {
    if [ -f "$PIDFILE" ] && kill -0 "$(cat "$PIDFILE")" 2>/dev/null; then
        echo "$NAME is running with PID $(cat "$PIDFILE")"
        exit 0
    fi
    echo "$NAME is not running"
    exit 3
}

case "$1" in
    start)   start ;;
    stop)    stop ;;
    restart) stop; start ;;
    status)  status ;;
    *) echo "Usage: $0 {start|stop|restart|status}"; exit 2 ;;
esac

exit 0
