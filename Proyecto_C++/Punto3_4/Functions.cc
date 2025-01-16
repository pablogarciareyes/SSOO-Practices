#include "Functions.h"

/// @brief Pasa los argumentos de la línea de comandos
/// @param argc 
/// @param argv 
/// @return options
// Implementación de parse_args
std::expected<program_options, parse_args_errors> parse_args(int argc, char* argv[]) {
    std::vector<std::string_view> args(argv + 1, argv + argc);
    program_options options;
    for (auto it = args.begin(), end = args.end(); it != end; ++it) {
        if (*it == "-h" || *it == "--help") {
            options.show_help = true;
        } else if (*it == "-v" || *it == "--verbose") {
            options.verbose = true;
        } else if (*it == "-p" || *it == "--port") {
            // Verifica que hay un valor después de -p
            if (++it == end || it->starts_with("-")) {
                return std::unexpected(parse_args_errors::missing_argument); // Error si no hay valor
            }
            // Almacenar el valor del puerto en options
            int port = std::atoi(it->data()); // Convierte el string view a int
            options.port_value = static_cast<uint16_t>(port); // Convierte el int a uint16_t
            options.port = true; 
        } else if (*it == "-b" || *it == "--base") {
            // Verificar que hay un valor después de -b
            if (++it == end || it->starts_with("-")) {
                return std::unexpected(parse_args_errors::missing_argument); // Error si no hay valor
            }
            // Comprobar que la ruta existe usando access()
            if (access(it->data(), F_OK) == -1) {
                return std::unexpected(parse_args_errors::invalid_route); // Error si no existe
            }
            // Almacenar la ruta en options
            options.ruta_base = std::string(it->data());
            options.base = true;
        } else {
            return std::unexpected(parse_args_errors::unknown_option);
        }
    }
    return options;
}


/// @brief Lee el contenido de un archivo
/// @param path
/// @return std::string
std::expected<SafeMap, int> read_all(const std::string& path) {
  int fd; // File descriptor
  SafeFD safe_fd{fd = open(path.c_str(), O_RDONLY)}; // Safe file descriptor

  // Comprobar si se ha abierto correctamente el archivo
  if (!safe_fd.is_valid()) { 
    return std::unexpected(errno);
  } 

  // Para mapear una archivo completo es necesario conocer su tamaño.
  // Una forma es usar fstat() y otra es usar lseek().
  // La función lseek() sirve para mover el puntero de lectura/escritura de un archivo y retorna la posición
  // a la que se ha movido. Por tanto, si se mueve al final del archivo, se obtiene el tamaño de este.
  int length = lseek(fd, 0, SEEK_END);

  // Se mapea el archivo completo en memoria para solo lectura y de forma privada
  void* mem = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);
  // Comprobar si se ha mapeado correctamente el archivo
  if (mem == MAP_FAILED) {
    return std::unexpected(errno);
  }

  // Opcionalmente, se puede cerrar el archivo después de mapearlo
  close(safe_fd.get());

  // Copiar los datos mapeados en un std::string_view
  std::string_view sv{static_cast<char*>(mem), static_cast<size_t>(length)};
  SafeMap safe_map{sv}; // Safe map

  return safe_map;
}

/// @brief Crea un socket
/// @param port
/// @return SafeFD
std::expected<SafeFD, int> make_socket(uint16_t port) {
  int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_fd < 0) {
    return std::unexpected(errno);
  }
  // BIND
  sockaddr_in local_address{};
  local_address.sin_family = AF_INET;
  local_address.sin_addr.s_addr = htonl(INADDR_ANY);
  local_address.sin_port = htons(port);
  int result = bind(sock_fd, reinterpret_cast<sockaddr*>(&local_address), sizeof(local_address));
  if (result < 0) {
    return std::unexpected(errno);
  }

  result = listen(sock_fd, 5);
  if (result < 0) {
    close(sock_fd);
    return std::unexpected(errno);
  }
  return SafeFD(sock_fd);
}

/// @brief Escucha conexiones
/// @param socket
/// @return int
int listen_connection(const SafeFD& socket) {
  int result = listen(socket.get(), 5);
  if (result < 0) {
    return errno;
  }
  return result;
}

/// @brief Acepta una conexión
/// @param socket
/// @param client_addr
/// @return SafeFD
std::expected<SafeFD, int> accept_connection(const SafeFD& socket, sockaddr_in& client_addr) {
  socklen_t client_addr_length = sizeof(client_addr);
  int new_fd = accept(socket.get(), reinterpret_cast<sockaddr*>(&client_addr), &client_addr_length);
  if (new_fd < 0) {
    return std::unexpected(errno);
  }
  return SafeFD(new_fd);
}

/// @brief Envia una respuesta
/// @param socket
/// @param header
/// @param body
/// @return int
int send_response(const SafeFD& socket, std::string_view header, std::string_view body) {
  std::string response = std::string(header) + std::string(body) + "\n";
  int bytes_sent = send(socket.get(), response.c_str(), response.size(), 0);
  if (bytes_sent < 0) {
    return errno;
  }
  return bytes_sent;
}

/// @brief Recibe una petición
/// @param socket
/// @param max_size
/// @return std::string
std::expected<std::string, int> receive_request(const SafeFD& socket, size_t max_size) {
  std::string buffer(max_size, '\0');
  int bytes_received = recv(socket.get(), buffer.data(), max_size, 0);
  if (bytes_received < 0) {
    return std::unexpected(errno);
  }
  buffer.resize(bytes_received);
  return buffer;
}

/// @brief Ejecuta un programa
/// @param path Ruta del programa
/// @param env Entorno de ejecución
/// @return Resultado de la ejecución o error
std::expected<std::string, execute_program_error> execute_program(const std::string& path, const exec_environment& env) {
    // Crear una tubería para capturar la salida estándar del proceso hijo
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        std::cerr << "Error: no se pudo crear la tubería\n";
        return std::unexpected(execute_program_error{.exit_code = -1, .error_code = errno});
    }

    // Comprobar que el programa existe y tenemos permisos con la función access()
    if (access(path.c_str(), X_OK) == -1) {
        return std::unexpected(execute_program_error{.exit_code = -1, .error_code = errno});
    }

    // Crear un proceso hijo con fork()
    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "Error: fallo al crear el proceso hijo con fork()\n";
        return std::unexpected(execute_program_error{.exit_code = 124, .error_code = errno});
    }

    // Flujo de ejecución proceso hijo
    if (pid == 0) {
        close(pipefd[0]); // Cerrar extremo de lectura
        if (dup2(pipefd[1], STDOUT_FILENO) == -1) { // Redirigir salida estándar
            std::cerr << "Error: fallo al redirigir la salida estándar\n";
            return std::unexpected(execute_program_error{.exit_code = 125, .error_code = errno});
        }
        close(pipefd[1]); // Cerrar extremo de escritura después de redirigir

        // Configurar las variables de entorno
        setenv("REQUEST_PATH", env.REQUEST_PATH.c_str(), 1);
        setenv("SERVER_BASEDIR", env.SERVER_BASEDIR.c_str(), 1);
        setenv("REMOTE_PORT", env.REMOTE_PORT.c_str(), 1);
        setenv("REMOTE_IP", env.REMOTE_IP.c_str(), 1);

        // Ejecutar el programa con execl()
        execl(path.c_str(), path.c_str(), nullptr);

        // Si execl() falla
        std::cerr << "Error: fallo al ejecutar el programa con execl()\n";
        exit(errno == ENOENT ? 127 : 126);
    }

    // Flujo de ejecución proceso padre
    close(pipefd[1]); // Cerrar extremo de escritura en el proceso padre

    // Leer la tubería con read() hasta que devuelva 0
    std::string output;
    while (true) {
        char buffer[4096];
        ssize_t bytes_read = read(pipefd[0], buffer, sizeof(buffer));
        if (bytes_read < 0) {
            std::cerr << "Error: fallo al leer de la tubería\n";
            close(pipefd[0]);
            return std::unexpected(execute_program_error{.exit_code = 126, .error_code = errno});
        }
        if (bytes_read == 0) {
            break;
        }
        output.append(buffer, bytes_read);
    }
    close(pipefd[0]);

    // Esperar a que el proceso hijo termine
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        std::cerr << "Error: fallo al esperar la finalización del proceso hijo (errno=" << errno << ")\n";
        return std::unexpected(execute_program_error{.exit_code = -1, .error_code = errno});
    }

    if (!WIFEXITED(status)) {
        std::cerr << "Error: el proceso hijo no terminó correctamente\n";
        return std::unexpected(execute_program_error{.exit_code = -1, .error_code = 0});
    }

    if (WEXITSTATUS(status) != 0) {
        std::cerr << "Error: el programa terminó con un código de salida distinto de 0: " << WEXITSTATUS(status) << "\n";
        return std::unexpected(execute_program_error{.exit_code = WEXITSTATUS(status), .error_code = 0});
    }
    // Devolver la salida estándar del proceso hijo
    return output;
}
