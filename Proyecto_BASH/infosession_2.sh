#!/bin/bash

# ./infosession_2.sh -u root usuario -z -e -d /home/usuario/ -t (ENTREGA 2)

# Función para mostrar la ayuda
function show_help() {
    echo "Uso: $0 [-h] [-z] [-u user1 user2 ... ] [-d dir] [-t] [-e] [-sm] [-sg] [-r]"
    echo "  -h       Muestra esta ayuda y termina"
    echo "  -z       Incluye procesos con sesión 0"
    echo "  -u user  Muestra procesos cuyo usuario efectivo sea el especificado (acepta múltiples usuarios)"
    echo "  -d dir   Filtra por procesos que tengan archivos abiertos en el directorio"
    echo "  -t       Filtra por procesos que tengan terminal asociada"
    echo "  -e       Muestra lista de procesos individuales en lugar de resumen por sesión"
    exit 0
}

# Variables de control
SHOW_HELP=0
INCLUDE_SID_0=0
USERS=()
DIRECTORY=""
FILTER_TTY=0
SUMMARY_MODE=0

# Procesar opciones de la línea de comandos
while [ -n "$1" ]; do
    case "$1" in
        # MOSTRAR AYUDA
        -h) SHOW_HELP=1; shift ;;
        # INCLUIR SESIÓN 0
        -z) INCLUDE_SID_0=1; shift ;;
        # FILTRAR POR VARIOS USUARIOS USANDO ARRAY
        -u)
            shift
            while [[ $# -gt 0 && "$1" != -* ]]; do # Mientras haya argumentos y no sea una opción
                USERS+=("$1") # Añadir usuario al array
                shift
            done ;;
        # FILTRAR POR DIRECTORIO
        -d) DIRECTORY="$2"; shift 2 ;; 
        # FILTRAR POR PROCESOS CON TTY
        -t) FILTER_TTY=1; shift ;;
        # MODO RESUMEN
        -e) SUMMARY_MODE=1; shift ;;
        *) echo "Opción inválida: -$1" >&2
           show_help ;;
    esac
done

# APARTADO 1 b)
if [ $SHOW_HELP -eq 1 ]; then
    show_help
fi

# Comando base de ps para obtener procesos
ps_command="ps -eo sid,pgid,pid,euser,tty,%mem,cmd"

# APARTADO 1 c)
if [ $INCLUDE_SID_0 -eq 0 ]; then
    ps_command="$ps_command | awk '\$1 != 0'"
fi

# APARTADO 1 d) + APARTADO 2 a)
if [ ${#USERS[@]} -gt 0 ]; then
    user_condition=$(printf '$4=="%s" || ' "${USERS[@]}") # Crear para usuario: $4=="usuario" ||
    user_condition=${user_condition% || }  # Quitar el último " || "
    ps_command="$ps_command | awk '$user_condition'" # Añadir condición al comando
fi

# Eval para poder guardar el comando en una única variable para futuros filtros
processes=$(eval "$ps_command")

# APARTADO 2 b)
if [ -n "$DIRECTORY" ]; then
    dir_processes=$(lsof +d "$DIRECTORY" | awk '{print $2}')
    processes=$(echo "$processes" | grep -w -f <(echo "$dir_processes"))
fi

# APARTADO 2 c)
if [ $FILTER_TTY -eq 1 ]; then
    processes=$(echo "$processes" | awk '$5 != "?"')
fi

# APARTADO 3 a)
if [ $SUMMARY_MODE -eq 1 ]; then
    # Modo lista de procesos individuales
    printf "%-5s | %-5s | %-5s | %-10s | %-5s | %-6s | %s\n" "SID" "PGID" "PID" "Usuario" "TTY" "% Mem" "Comando"
    echo "$processes" | awk '{ printf "%-5s | %-5s | %-5s | %-10s | %-5s | %-6.1f | %s\n", $1, $2, $3, $4, $5, $6, $7 }'
else
    # Modo tabla de resumen de sesiones
    printf "%-5s | %-12s | %-10s | %-15s | %-10s | %-5s | %s\n" "SID" "Total Grupos" "%MEM Total" "SID Lider" "USER Lider" "TTY" "Comando"
    echo "$processes" | awk '
    {
        # Variables de proceso
        sid = $1
        pgid = $2
        pid = $3
        user = $4
        tty = $5
        mem = $6
        cmd = $7

        # Calcular totales por sesión
        mem_total[sid] += mem           # Sumar memoria al array con clave sid
        groups[sid][pgid] = 1           # Usar el array con dos claves para contar únicos
        if (!leader[sid]) {             # Si no hay líder
            leader[sid] = pid           # Asignar líder
            leader_user[sid] = user     # Asignar usuario líder
            leader_tty[sid] = tty       # Asignar tty líder
            leader_cmd[sid] = cmd       # Asignar comando líder
        }
    }
    END {
        for (sid in mem_total) { # Recorrer todas los SIDs
            group_count = length(groups[sid]) # Contar grupos únicos según el tamaño del array con cierto SID
            printf "%-5s | %-12s | %-10s | %-15s | %-10s | %-5s | %s\n", sid, group_count, mem_total[sid], leader[sid], leader_user[sid] ? leader_user[sid] : "?", leader_tty[sid] ? leader_tty[sid] : "?", leader_cmd[sid] ? leader_cmd[sid] : "?"
        }
    }'
fi