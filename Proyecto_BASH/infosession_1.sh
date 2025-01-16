#!/bin/bash

# ./infosession.sh -u pablo -d /home/pablo -z (ENTREGA 1)

# Función para mostrar la ayuda
function show_help() {
    echo "Uso: $0 [-h] [-z] [-u user1 ... ] [-d dir] [-t] [-e] [-sm] [-sg] [-r]"
    echo "  -h       Muestra esta ayuda y termina"
    echo "  -z       Incluye procesos con sesión 0"
    echo "  -u user  Muestra procesos cuyo usuario efectivo sea el especificado"
    echo "  -d dir   Filtra por procesos que tengan archivos abiertos en el directorio"
    exit 1
}

# Variables de control
SHOW_HELP=0
INCLUDE_SID_0=0
USERS=()
DIRECTORY=""

# Procesar opciones de la línea de comandos
while getopts "hzu:d:tesmgr" opt; do
    case ${opt} in
        h) SHOW_HELP=1 ;;
        z) INCLUDE_SID_0=1 ;;
        u) USERS+=("$OPTARG") ;;
        d) DIRECTORY="$OPTARG" ;;
        *) echo "Opción inválida: -$OPTARG" >&2
           show_help ;;
    esac
done

# Mostrar ayuda si se pide
if [ $SHOW_HELP -eq 1 ]; then
    show_help
fi

# Comando base de ps para obtener procesos
ps_command="ps -eo sid,pgid,pid,euser,tty,%mem,cmd"

# Si no se quiere incluir SID 0, filtrar con awk
if [ $INCLUDE_SID_0 -eq 0 ]; then
    ps_command="$ps_command | awk '\$1 != 0'" # Comprobar si el SID es distinto de 0 para no guardarlo en el comando
fi

# Filtrar por usuarios si es necesario
if [ ${#USERS[@]} -gt 0 ]; then
    # Crear un filtro que coincide con los usuarios especificados
    user_filter=$(printf "${USERS[@]}")
    ps_command="$ps_command | awk '\$4 ~ /^(${user_filter%|})$/'" # Comprobar si los usuarios de la columna 4 coinciden con alguno de los especificados
fi

# Ejecutar el comando ps para obtener los datos
processes=$(eval "$ps_command")

# Filtrar por directorio usando lsof si se especificó
if [ -n "$DIRECTORY" ]; then
    # Obtener los PIDs de los procesos que tienen archivos abiertos en el directorio
    pids=$(lsof +d "$DIRECTORY" -Fp | grep -o '[0-9]*' | tr '\n' '|' | sed 's/|$//') # Devuelve los PIDs de la manera 1234|5678|91011
    # Filtrar los procesos que están en la lista de PIDs
    if [ -n "$pids" ]; then
        processes=$(echo "$processes" | awk -v pids="^($pids)$" '$3 ~ pids') # Comprueba los PIDs que coinciden con la lista
    else
        processes=""
    fi
fi

# Mostrar los procesos en un formato legible
printf "%-5s | %-5s | %-5s | %-10s | %-5s | %-6s | %s\n" "SID" "PGID" "PID" "Usuario" "TTY" "% Mem" "Comando"
# Imprimir los procesos formateados si hay resultados
if [ -n "$processes" ]; then
    echo "$processes" | awk '{ printf "%-5s | %-5s | %-5s | %-10s | %-5s | %-6.1f | %s\n", $1, $2, $3, $4, $5, $6, $7 }'
else
    echo "No hay procesos que cumplan con los criterios especificados."
fi