
#!/bin/sh
# Tester script for assignment 1, 2, 3, 4
# Autor original: Siddhant Jajoo
# Modificado por: Alfonso + ajustes A4 para PATH y /etc/finder-app/conf

set -eu

# Valores por defecto (si no se pasan argumentos)
NUMFILES=10
WRITESTR="AELD_IS_FUN"
WRITEDIR="/tmp/aeld-data"

# Config en imagen (debe existir en el target rootfs)
CONF_DIR="/etc/finder-app/conf"
USERNAME_FILE="${CONF_DIR}/username.txt"
ASSIGNMENT_FILE="${CONF_DIR}/assignment.txt"
RESULT_FILE="/tmp/assignment4-result.txt"


# --- Validaciones de entorno y dependencias ---
# writer y finder.sh deben estar en el PATH (/usr/bin en la imagen)
if ! command -v writer >/dev/null 2>&1; then
    echo "failed: 'writer' no está en PATH. Asegúrate de que el paquete aesd-assignments lo instale en /usr/bin."
    exit 1
fi

if ! command -v finder.sh >/dev/null 2>&1; then
    echo "failed: 'finder.sh' no está en PATH. Asegúrate de que el paquete aesd-assignments lo instale en /usr/bin."
    exit 1
fi

# Ficheros de configuración obligatorios
if [ ! -r "${USERNAME_FILE}" ]; then
    echo "failed: no se puede leer ${USERNAME_FILE}"
    exit 1
fi
if [ ! -r "${ASSIGNMENT_FILE}" ]; then
    echo "failed: no se puede leer ${ASSIGNMENT_FILE}"
    exit 1
fi

username="$(cat "${USERNAME_FILE}")"
assignment="$(cat "${ASSIGNMENT_FILE}")"

# --- Parámetros ---
if [ $# -lt 3 ]; then
    echo "Usando valor por defecto WRITESTR=${WRITESTR}"
    if [ $# -lt 1 ]; then
        echo "Usando valor por defecto NUMFILES=${NUMFILES}"
    else
        NUMFILES="$1"
    fi
else
    NUMFILES="$1"
    WRITESTR="$2"
    WRITEDIR="/tmp/aeld-data/$3"
fi

MATCHSTR="The number of files are ${NUMFILES} and the number of matching lines are ${NUMFILES}"

echo "Writing ${NUMFILES} files containing string '${WRITESTR}' to '${WRITEDIR}'"

# Limpiar y preparar directorio de escritura
rm -rf "${WRITEDIR}"
# Si NO es assignment1, crear el directorio (A2 lo requiere)
if [ "${assignment}" != "assignment1" ]; then
    mkdir -p "${WRITEDIR}"
    if [ ! -d "${WRITEDIR}" ]; then
        echo "failed: no se pudo crear ${WRITEDIR}"
        exit 1
    fi
fi

# Escribir archivos usando 'writer' del PATH
i=1
while [ "${i}" -le "${NUMFILES}" ]; do
    writer "${WRITEDIR}/${username}${i}.txt" "${WRITESTR}"
    i=$((i + 1))
done

# Ejecutar finder.sh (desde PATH), capturar salida y guardar en /tmp/assignment4-result.txt
OUTPUTSTRING="$(finder.sh "${WRITEDIR}" "${WRITESTR}")"

# Reemplazar el resultado anterior (si lo hay)
rm -f "${RESULT_FILE}"
printf "%s\n" "${OUTPUTSTRING}" > "${RESULT_FILE}"

# Verificación
echo "${OUTPUTSTRING}" | grep -F "${MATCHSTR}" >/dev/null 2>&1
if [ $? -eq 0 ]; then
    echo "success"
    exit 0
else
    echo "failed: expected '${MATCHSTR}' in '${OUTPUTSTRING}'"
    exit 1
fi
