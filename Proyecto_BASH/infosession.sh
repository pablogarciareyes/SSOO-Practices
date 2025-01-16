#!/bin/bash

# 
# 2º Grado en Ingeniería Informática
# Proyecto BASH - Sistemas Operativos
# Fecha de correción: XX/11/2024
# Horario de correción:

# Función para mostrar la ayuda
function show_help() {
    echo "Uso: $0 [-h] [-z] [-u user1 user2 ... ] [-d dir] [-t] [-e] [-sm] [-sg] [-r]"
    echo "  -h       Muestra esta ayuda y termina"
    echo "  -z       Incluye procesos con sesión 0"
    echo "  -u user  Muestra procesos cuyo usuario efectivo sea el especificado (acepta múltiples usuarios)"
    echo "  -d dir   Filtra por procesos que tengan archivos abiertos en el directorio"
    echo "  -t       Filtra por procesos que tengan terminal asociada"
    echo "  -e       Muestra lista de procesos individuales en lugar de resumen por sesión"
    echo "  -sm      Ordena la salida por memoria"
    echo "  -sg      Ordena la salida por grupos"
    echo "  -r       Ordena la salida de forma inversa"
    exit 2
}

# Variables de control
SHOW_HELP=0
INCLUDE_SID_0=0
USERS=()
DIRECTORY=""
FILTER_TTY=0
NORMAL_MODE=0
SORT_MEM=0
SORT_GROUPS=0
REVERSE=0

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
            # COMPROBACIÓN DE ERROR: Se esperaba al menos un usuario
            if [ -z "$1" ]; then
                echo "Se esperaba al menos un usuario" >&2
                show_help
            fi
            # AÑADIR USUARIOS AL ARRAY
            while [[ $# -gt 0 && "$1" != -* ]]; do # Mientras haya argumentos y no sea una opción
                USERS+=("$1") # Añadir usuario al array
                shift
            done ;;
        # FILTRAR POR DIRECTORIO
        -d) DIRECTORY="$2"; shift 2 ;;
        # FILTRAR POR PROCESOS CON TTY
        -t) FILTER_TTY=1; shift ;;
        # MODO RESUMEN
        -e) NORMAL_MODE=1; shift ;;
        # ORDENAR POR MEMORIA
        -sm) SORT_MEM=1; shift ;;
        # ORDENAR POR GRUPOS
        -sg) SORT_GROUPS=1; shift ;;
        # ORDENAR INVERSO
        -r) REVERSE=1; shift ;;
        *) echo "Opción inválida: -$1" >&2
           show_help ;;
    esac
done

# COMPROBACIONES DE ERROR

# COMPROBACIÓN: -sm y -sg no compatibles
if [ $SORT_MEM -eq 1 ] && [ $SORT_GROUPS -eq 1 ]; then
     echo "No se puede ordenar por memoria y por grupos simultáneamente" >&2
    exit 1
fi

# COMPROBACIÓN: -sg y -e no compatibles
if [ $SORT_GROUPS -eq 1 ] && [ $NORMAL_MODE -eq 1 ]; then
    echo "No se puede ordenar por grupos si no es en el modo resumen" >&2
    exit 1
fi

# ----------------------------------------------

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
    ps_command="$ps_command | awk '$user_condition'" # Añadir filtro al comando
fi

# Eval para poder guardar el comando en una única variable para futuros filtros
processes=$(eval "$ps_command")

# APARTADO 2 b)
if [ -n "$DIRECTORY" ]; then
    dir_processes=$(lsof +d "$DIRECTORY" | awk '{print $2}') # Cogemos la columna de los PIDs
    processes=$(echo "$processes" | grep -w -f <(echo "$dir_processes")) # Filtramos los procesos
fi

# APARTADO 2 c)
if [ $FILTER_TTY -eq 1 ]; then
    processes=$(echo "$processes" | awk '$5 != "?"')
fi

# APARTADO 3 a)
if [ $NORMAL_MODE -eq 1 ]; then
    # Modo lista de procesos individuales
    printf "%-7s | %-7s | %-7s | %-10s | %-5s | %-6s | %s\n" "SID" "PGID" "PID" "Usuario" "TTY" "% Mem" "Comando"
    tabla=$(echo "$processes" | awk '{ printf "%-7s | %-7s | %-7s | %-10s | %-5s | %-6.1f | %s\n", $1, $2, $3, $4, $5, $6, $7 }')

    # Ordenar por memoria FUNCIONA
    if [ $SORT_MEM -eq 1 ] && [ $SORT_GROUPS -eq 0 ] && [ $REVERSE -eq 0 ]; then
        echo -e "$tabla" | LC_ALL=C sort -t "|" -k 6 -n
        exit 0
    fi

    # Ordenar por memoria inverso FUNCIONA
    if [ $SORT_MEM -eq 1 ] && [ $REVERSE -eq 1 ] && [ $SORT_GROUPS -eq 0 ]; then
        echo -e "$tabla" | LC_ALL=C sort -t "|" -k 6 -n -r
        exit 0
    fi

    # Ordenar por usuarios inverso FUNCIONA
    if [ $REVERSE -eq 1 ] && [ $SORT_MEM -eq 0 ] && [ $SORT_GROUPS -eq 0 ]; then
        echo -e "$tabla" | sort -t "|" -k 4 -r
        exit 0
    fi

    # Ordenar la tabla por usuario si no se especifica ninguna opción de ordenamiento
    echo -e "$tabla" | sort -t "|" -k 4
    exit 0
else
    # Modo tabla de resumen de sesiones
    printf "%-7s | %-12s | %-10s | %-15s | %-10s | %-5s | %s\n" "SID" "Total Grupos" "%MEM Total" "PID Lider" "USER Lider" "TTY" "Comando"
    SIDs=$(echo "$processes" | awk '{print $1}' | sort -n | uniq)
    for sid_actual in $SIDs; do
        # Filtrar los procesos actuales por SID
        procesos_sid=$(echo "$processes" | awk -v SID="$sid_actual" '$1 == SID')
        # Calcular el número de grupos únicos (PGID)
        numero_pgid=$(echo "$procesos_sid" | awk '{print $2}' | sort -n | uniq | wc -l)
        # Sumar la memoria total usada por procesos con el mismo SID
        suma_memoria=$(echo "$procesos_sid" | LC_ALL=C awk '{mem += $6} END {print mem}')
        # Determinar el proceso líder (proceso cuyo PID coincide con el SID)
        lider=$(echo "$procesos_sid" | awk -v SID=" $sid_actual" '$3 == SID') # PID_LIDER = SID
        # Extraer los datos del proceso líder
        pid_lider=$(echo "$lider" | awk '{print $2}') # PGID = PID_LIDER
        usuario_lider=$(echo "$lider" | awk '{print $4}')
        terminal_lider=$(echo "$lider" | awk '{print $5}')
        cmd_lider=$(echo "$lider" | awk '{print $7}')
        # Imprimir por orden de usuario
        tabla+=$(printf "%-7s | %-12s | %-10s | %-15s | %-10s | %-5s | %s" "$sid_actual" "$numero_pgid" "$suma_memoria" "${pid_lider:-?}" "${usuario_lider:-?}" "${terminal_lider:-?}" "${cmd_lider:-?}")
        tabla+="\n"
    done
    
    # 3 b)
    # Ordenar por memoria FUNCIONA
    if [ $SORT_MEM -eq 1 ] && [ $SORT_GROUPS -eq 0 ] && [ $REVERSE -eq 0 ]; then
        echo -e "$tabla" | sort -t "|" -k 3
        exit 0
    fi

    # 3 c)
    # Ordenar por grupos FUNCIONA
    if [ $SORT_MEM -eq 0 ] && [ $SORT_GROUPS -eq 1 ] && [ $REVERSE -eq 0 ]; then
        echo -e "$tabla" | sort -t "|" -k 2
        exit 0
    fi

    # 3 d)
    # Ordenar inverso por usuarios FUNCIONA
    if [ $REVERSE -eq 1 ] && [ $SORT_MEM -eq 0 ] && [ $SORT_GROUPS -eq 0 ]; then
        echo -e "$tabla" | sort -t "|" -k 5 -r
        exit 0
    fi

    # Ordenar inverso por memoria FUNCIONA
    if [ $REVERSE -eq 1 ] && [ $SORT_MEM -eq 1 ] && [ $SORT_GROUPS -eq 0 ]; then
        echo -e "$tabla" | sort -t "|" -k 3 -r
        exit 0
    fi

    # Ordenar inverso por grupos FUNCIONA
    if [ $REVERSE -eq 1 ] && [ $SORT_MEM -eq 0 ] && [ $SORT_GROUPS -eq 1 ]; then
        echo -e "$tabla" | sort -t "|" -k 2 -r
        exit 0
    fi

    # Ordenar la tabla por usuario si no se especifica ninguna opción de ordenamiento
    echo -e "$tabla" | sort -t "|" -k 5
    exit 0
fi