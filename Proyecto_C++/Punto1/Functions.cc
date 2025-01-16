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
        } else if(*it == "-n") {
            options.no_size = true;
        } else if (*it == "-v" || *it == "--verbose") {
            options.verbose = true;
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

  // Opcionalmente, se puede cerrar el archivo después de mapearlo (aunque el destructor de la clase lo hace automáticamente)
  close(safe_fd.get());

  // Copiar los datos mapeados en un std::string_view
  std::string_view sv{static_cast<char*>(mem), static_cast<size_t>(length)};
  SafeMap safe_map{sv}; // Lo guardamos en un SafeMap

  return safe_map;
}

/// @brief Muestra el contenido de un archivo
/// @param header
/// @param body
void send_response(std::string_view header, std::string_view body) {
  std::cout << header << body << std::endl;
}