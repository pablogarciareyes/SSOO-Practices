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
            // Verificar que hay un valor después de -p
            if (++it == end) {
                return std::unexpected(parse_args_errors::missing_argument); // Error si no hay valor
            }
            // Almacenar el valor del puerto en una variable
            int port = std::atoi(it->data());
            options.port_value = static_cast<uint16_t>(port);
            options.port = true;
        } else if (!it->starts_with("-")) {
            options.output_filename = std::string(*it);
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