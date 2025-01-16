#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <iostream>
#include <vector>
#include <string>
#include <string_view>
#include <expected>
#include <format>
#include <bits/stdc++.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "SafeFD.h"
#include "SafeMap.h"

// Enumerado para los errores de parse_args
enum class parse_args_errors
{
  missing_argument,
  unknown_option,
  // ...
};

// Estructura para almacenar las opciones del programa
struct program_options 
{
  bool show_help = false;
  bool verbose = false;
  bool no_size = false;
  std::string output_filename;
  // ...
  std::vector<std::string> additional_args; 
};

std::expected<program_options, parse_args_errors> parse_args(int argc, char* argv[]);
std::expected<SafeMap, int> read_all(const std::string& path);
void send_response(std::string_view header, std::string_view body = {});

#endif