
// server/aesdsocket.c
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#define PORT 9000
#define BACKLOG 10
#define DATAFILE "/var/tmp/aesdsocketdata"

static volatile sig_atomic_t g_exit_requested = 0;
static int g_listen_fd = -1;

/**
 * Manejo de señales SIGINT/SIGTERM: señalamos salida y cerramos listen_fd
 * para desbloquear accept().
 */
static void signal_handler(int signum)
{
    (void)signum;
    g_exit_requested = 1;
    syslog(LOG_INFO, "Caught signal, exiting");
    if (g_listen_fd >= 0)
    {
        close(g_listen_fd); // fuerza fallo en accept() y permite salir del loop
        g_listen_fd = -1;
    }
}

/**
 * Instala manejadores para SIGINT y SIGTERM.
 */
static void install_signal_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // no SA_RESTART: queremos que accept/read se interrumpan
    if (sigaction(SIGINT, &sa, NULL) < 0)
    {
        perror("sigaction(SIGINT)");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGTERM, &sa, NULL) < 0)
    {
        perror("sigaction(SIGTERM)");
        exit(EXIT_FAILURE);
    }
}

/**
 * Crea y bindea el socket en PORT. Devuelve fd o -1 en error.
 */
static int create_and_bind_socket(void)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        syslog(LOG_ERR, "socket() failed: %s", strerror(errno));
        return -1;
    }

    int optval = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
    {
        syslog(LOG_ERR, "setsockopt(SO_REUSEADDR) failed: %s", strerror(errno));
        close(fd);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        syslog(LOG_ERR, "bind() failed: %s", strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

/**
 * Daemoniza el proceso: fork, setsid, cerrar stdio, umask, chdir.
 * Debe llamarse DESPUÉS de bind (según requisitos).
 */
static void daemonize(void)
{
    pid_t pid = fork();
    if (pid < 0)
    {
        syslog(LOG_ERR, "fork() failed during daemonize: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (pid > 0)
    {
        // Proceso padre sale
        exit(EXIT_SUCCESS);
    }

    // Hijo
    if (setsid() < 0)
    {
        syslog(LOG_ERR, "setsid() failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Opcional: segundo fork para evitar volver a adquirir terminal
    // (no estrictamente necesario aquí)
    // pid = fork();
    // if (pid < 0) exit(EXIT_FAILURE);
    // if (pid > 0) exit(EXIT_SUCCESS);

    umask(0);
    if (chdir("/") < 0)
    {
        // No crítico, pero recomendable
        syslog(LOG_WARNING, "chdir(\"/\") failed: %s", strerror(errno));
    }

    // Cerrar stdin/stdout/stderr
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    // Redirigir a /dev/null
    int fdnull = open("/dev/null", O_RDWR);
    if (fdnull >= 0)
    {
        dup2(fdnull, STDIN_FILENO);
        dup2(fdnull, STDOUT_FILENO);
        dup2(fdnull, STDERR_FILENO);
        if (fdnull > STDERR_FILENO)
            close(fdnull);
    }
}

/**
 * Asegura que DATAFILE exista y sea accesible. No abre persistente.
 */
static int ensure_datafile(void)
{
    int fd = open(DATAFILE, O_CREAT | O_APPEND | O_RDWR, 0644);
    if (fd < 0)
    {
        syslog(LOG_ERR, "Failed to open/create %s: %s", DATAFILE, strerror(errno));
        return -1;
    }
    close(fd);
    return 0;
}

/**
 * Append de un paquete (línea terminada en '\n') al DATAFILE.
 * packet incluye el '\n'.
 */
static int append_packet_to_file(const char *packet, size_t len)
{
    int fd = open(DATAFILE, O_CREAT | O_APPEND | O_WRONLY, 0644);
    if (fd < 0)
    {
        syslog(LOG_ERR, "open(%s) for append failed: %s", DATAFILE, strerror(errno));
        return -1;
    }

    // (Opcional) bloquear archivo para consistencia si multicliente (no requerido aquí)
    // flock(fd, LOCK_EX);

    ssize_t written = 0;
    while (written < (ssize_t)len)
    {
        ssize_t rc = write(fd, packet + written, len - written);
        if (rc < 0)
        {
            syslog(LOG_ERR, "write(%s) failed: %s", DATAFILE, strerror(errno));
            close(fd);
            return -1;
        }
        written += rc;
    }

    // flock(fd, LOCK_UN);
    close(fd);
    return 0;
}

/**
 * Envía el contenido completo del DATAFILE al cliente.
 */
static int send_file_to_client(int client_fd)
{
    int fd = open(DATAFILE, O_RDONLY);
    if (fd < 0)
    {
        syslog(LOG_ERR, "open(%s) for read failed: %s", DATAFILE, strerror(errno));
        return -1;
    }

    char buf[4096];
    ssize_t rd;
    while ((rd = read(fd, buf, sizeof(buf))) > 0)
    {
        ssize_t sent = 0;
        while (sent < rd)
        {
            ssize_t rc = send(client_fd, buf + sent, rd - sent, 0);
            if (rc < 0)
            {
                syslog(LOG_ERR, "send() failed: %s", strerror(errno));
                close(fd);
                return -1;
            }
            sent += rc;
        }
    }
    if (rd < 0)
    {
        syslog(LOG_ERR, "read(%s) failed: %s", DATAFILE, strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

/**
 * Procesa una conexión: lee datos hasta que el cliente cierre; por cada
 * paquete terminado en '\n' lo agrega al DATAFILE y responde con el contenido completo.
 */
static void handle_connection(int client_fd, struct sockaddr_in *client_addr)
{
    char ipstr[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &(client_addr->sin_addr), ipstr, sizeof(ipstr));
    syslog(LOG_INFO, "Accepted connection from %s", ipstr);

    // Buffer de recepción y acumulador de datos para detectar '\n'
    char buf[4096];
    char *acc = NULL;
    size_t acc_len = 0;

    // Asegurar que el archivo existe
    if (ensure_datafile() < 0)
    {
        // No devolvemos a cliente; pero cerramos
        goto cleanup;
    }

    // Leer hasta cierre del cliente o señal
    while (!g_exit_requested)
    {
        ssize_t rd = recv(client_fd, buf, sizeof(buf), 0);
        if (rd < 0)
        {
            if (errno == EINTR && g_exit_requested)
                break;
            syslog(LOG_ERR, "recv() failed: %s", strerror(errno));
            break;
        }
        else if (rd == 0)
        {
            // Cliente cerró
            break;
        }

        // Acumular
        char *new_acc = realloc(acc, acc_len + rd);
        if (!new_acc)
        {
            syslog(LOG_ERR, "realloc() failed: %s", strerror(errno));
            // Descartamos datos por falta de memoria (permite requisitos)
            continue;
        }
        acc = new_acc;
        memcpy(acc + acc_len, buf, rd);
        acc_len += rd;

        // Procesar todas las líneas completas que existan en el acumulador
        size_t start = 0;
        for (size_t i = 0; i < acc_len; i++)
        {
            if (acc[i] == '\n')
            {
                size_t pkt_len = i - start + 1; // incluye '\n'
                if (append_packet_to_file(acc + start, pkt_len) == 0)
                {
                    // Enviar contenido completo del archivo
                    if (send_file_to_client(client_fd) != 0)
                    {
                        // Fallo al enviar; terminamos
                        start = i + 1;
                        goto done_processing;
                    }
                }
                // Avanzar al siguiente posible paquete
                start = i + 1;
            }
        }

        // Compactar buffer acumulador (dejar resto no procesado)
        if (start > 0)
        {
            size_t remaining = acc_len - start;
            memmove(acc, acc + start, remaining);
            acc_len = remaining;
            char *shrunk = realloc(acc, acc_len);
            if (shrunk)
                acc = shrunk; // OK si NULL: mantenemos memoria
        }
    }

done_processing:
cleanup:
    free(acc);
    syslog(LOG_INFO, "Closed connection from %s", ipstr);
    close(client_fd);
}

/**
 * Cierra recursos y elimina DATAFILE.
 */
static void cleanup_and_exit(int status)
{
    if (g_listen_fd >= 0)
    {
        close(g_listen_fd);
        g_listen_fd = -1;
    }
    // Eliminar datafile
    if (unlink(DATAFILE) < 0 && errno != ENOENT)
    {
        syslog(LOG_ERR, "unlink(%s) failed: %s", DATAFILE, strerror(errno));
    }
    closelog();
    exit(status);
}

int main(int argc, char *argv[])
{
    bool daemon_mode = false;
    if (argc == 2 && strcmp(argv[1], "-d") == 0)
    {
        daemon_mode = true;
    }
    else if (argc > 1)
    {
        fprintf(stderr, "Usage: %s [-d]\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Abrir syslog
    openlog("aesdsocket", LOG_PID, LOG_USER);

    install_signal_handlers();

    // Crear y bindear socket
    g_listen_fd = create_and_bind_socket();
    if (g_listen_fd < 0)
    {
        // Requisito: devolver -1 en error de pasos de socket
        closelog();
        return -1;
    }

    // Daemonizar si corresponde (después de bind)
    if (daemon_mode)
    {
        daemonize();
        // En modo daemon seguimos con el listen/accept en el proceso hijo
        openlog("aesdsocket", LOG_PID, LOG_USER); // reabrir por si cerró fds
    }

    if (listen(g_listen_fd, BACKLOG) < 0)
    {
        syslog(LOG_ERR, "listen() failed: %s", strerror(errno));
        cleanup_and_exit(-1);
    }

    // Loop principal: aceptar conexiones hasta señal
    while (!g_exit_requested)
    {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int client_fd = accept(g_listen_fd, (struct sockaddr *)&client_addr, &addrlen);
        if (client_fd < 0)
        {
            if (errno == EINTR && g_exit_requested)
                break;
            // Si g_listen_fd se cerró por señal, accept fallará
            if (g_exit_requested)
                break;
            syslog(LOG_ERR, "accept() failed: %s", strerror(errno));
            // continuar intentando
            continue;
        }

        // Procesar conectando cliente (modo single-thread, secuencial)
        handle_connection(client_fd, &client_addr);
    }

    cleanup_and_exit(EXIT_SUCCESS);
}
