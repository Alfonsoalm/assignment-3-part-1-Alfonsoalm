
#!/bin/sh
# aesdsocket-start-stop: Arranca/para aesdsocket como daemon usando start-stop-daemon
# Colocar en: assignments-3-and-later/server/aesdsocket-start-stop
# Instalado por Buildroot en: /etc/init.d/S99aesdsocket

### BEGIN INIT INFO
# Provides:          aesdsocket
# Required-Start:    $remote_fs $syslog $time
# Required-Stop:     $remote_fs $syslog $time
# Default-Start:     S
# Default-Stop:      0 6
# Short-Description: AESD socket daemon
### END INIT INFO

DAEMON="/usr/bin/aesdsocket"
DAEMON_OPTS="-d"
PIDFILE="/var/run/aesdsocket.pid"
NAME="aesdsocket"
DESC="AESD socket server"
DATAFILE="/var/tmp/aesdsocketdata"

# BusyBox init ejecuta scripts S?? en arranque y K?? en parada.
# Este script soporta: start|stop|restart|status

[ -x "$DAEMON" ] || { echo "$DAEMON no existe o no es ejecutable"; exit 1; }

ensure_dirs() {
    # /var/run puede ser un tmpfs (se vacía al arrancar)
    [ -d /var/run ] || mkdir -p /var/run
    # En algunas imágenes /var/tmp podría no existir
    [ -d /var/tmp ] || mkdir -p /var/tmp
}

start() {
    ensure_dirs
    echo "Starting $DESC..."
    start-stop-daemon -S -b -m -p "$PIDFILE" --exec "$DAEMON" -- $DAEMON_OPTS
    RET=$?
    if [ $RET -eq 0 ]; then
        echo "$NAME started"
    else
        echo "Failed to start $NAME (code $RET)"
    fi
    return $RET
}

stop() {
    echo "Stopping $DESC..."
    if [ -f "$PIDFILE" ]; then
        start-stop-daemon -K -p "$PIDFILE" -s TERM
        # Espera graciosa hasta 5s
        TIMEOUT=50
        while [ $TIMEOUT -gt 0 ] && [ -e "$PIDFILE" ]; do
            sleep 0.1
            TIMEOUT=$((TIMEOUT - 1))
        done
        [ -e "$PIDFILE" ] && echo "Warning: pidfile persists after stop"
        # No borres DATAFILE aquí: el propio daemon lo borra en SIGTERM.
    else
        # Fallback: intenta matar por ejecutable si no hay pidfile
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
    *)       echo "Usage: $0 {start|stop|restart|status}"; exit 2 ;;
esac

exit 0
