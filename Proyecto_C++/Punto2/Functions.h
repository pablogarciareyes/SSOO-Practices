#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <iostream>
#include <vector>
#include <string> 
#include <cerrno>
#include <expected>
#include <format>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sstream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "SafeFD.h"
#include "SafeMap.h"

// Enumerado para los errores de parse_args
enum class parse_args_errors
{
  missing_argument,
  unknown_option,
  invalid_port

};

// Estructura para almacenar las opciones del programa
struct program_options 
{
  bool show_help = false;
  bool verbose = false;
  bool port = false;
  uint16_t port_value = 0;
  std::string output_filename;
  // ...
  std::vector<std::string> additional_args; 
};

std::expected<program_options, parse_args_errors> parse_args(int argc, char* argv[]);
std::expected<SafeMap, int> read_all(const std::string& path);
std::expected<SafeFD, int> make_socket(uint16_t port);
int listen_connection(const SafeFD& socket);
std::expected<SafeFD, int> accept_connection(const SafeFD& socket, sockaddr_in& client_addr);
int send_response(const SafeFD& socket, std::string_view header, std::string_view body = {});


#endif