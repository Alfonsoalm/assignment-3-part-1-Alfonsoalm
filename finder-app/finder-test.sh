
#!/bin/sh
# Tester script for assignment 1 and assignment 2
# Author: Siddhant Jajoo
# Modificado por Alfonso: usa la utilidad C 'writer' sin compilar aquí (A3 part 1)

set -e
set -u

NUMFILES=10
WRITESTR=AELD_IS_FUN
WRITEDIR=/tmp/aeld-data
username=$(cat conf/username.txt)

if [ $# -lt 3 ]
then
    echo "Using default value ${WRITESTR} for string to write"
    if [ $# -lt 1 ]
    then
        echo "Using default value ${NUMFILES} for number of files to write"
    else
        NUMFILES=$1
    fi
else
    NUMFILES=$1
    WRITESTR=$2
    WRITEDIR=/tmp/aeld-data/$3
fi

MATCHSTR="The number of files are ${NUMFILES} and the number of matching lines are ${NUMFILES}"

echo "Writing ${NUMFILES} files containing string ${WRITESTR} to ${WRITEDIR}"

# Limpia el directorio de trabajo de datos previo
rm -rf "${WRITEDIR}"

# Detecta asignación actual
assignment=$(cat conf/assignment.txt)

# Si no es assignment1, crea el directorio (assignment2 lo requiere)
if [ "${assignment}" != "assignment1" ]
then
    mkdir -p "${WRITEDIR}"
    if [ -d "${WRITEDIR}" ]
    then
        echo "${WRITEDIR} created"
    else
        exit 1
    fi
fi

# *** IMPORTANTE: NO compilar en Assignment 3 Part 1 ***
# Verificación suave de binario existente
if [ ! -x "./writer" ]; then
    echo "failed: writer binary not found or not executable. Build it separately (Part 2)."
    exit 1
fi

# Escribir archivos usando la utilidad C 'writer'
for i in $(seq 1 "${NUMFILES}")
do
    ./writer "${WRITEDIR}/${username}${i}.txt" "${WRITESTR}"
done

# Ejecutar finder (script existente) para contar coincidencias
OUTPUTSTRING=$(sh ./finder.sh "${WRITEDIR}" "${WRITESTR}")

# remove temporary directories
rm -rf /tmp/aeld-data

set +e
echo "${OUTPUTSTRING}" | grep "${MATCHSTR}" > /dev/null
if [ $? -eq 0 ]; then
    echo "success"
    exit 0
else
    echo "failed: expected  ${MATCHSTR} in ${OUTPUTSTRING} but instead found"
    exit 1
fi

# Al final del script
echo "Final result" > /tmp/assignment4-result.txt