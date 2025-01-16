/**
 * Universidad de La Laguna
 * Escuela Superior de Ingeniería y Tecnología
 * Asignatura: Sistemas Operativos (SSOO)
 * Curso: 2º
 * Proyecto C++: Servidor de Documentos
 * @author 
 * @file docserver.cc
 * @brief docserver [-v | --verbose] [-h | --help] ARCHIVO
 * @bug No hay bugs conocidos
 * MODIFICACIÓN: Nuevo parámetro -n que si ponemos ese parámetro no sale el tamaño del archivo en pantalla
 *     
 * Compilar con: g++ -std=c++23 docserver.cc Functions.cc
*/

#include <iostream>
#include <vector>
#include <string> 
// Gestion de errores          
#include <cerrno>
#include <expected>
#include <format>
// Gestion de archivos
#include <bits/stdc++.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sstream>

#include "Functions.h"

int main(int argc, char* argv[]) {
  // PASAR ARGUMENTOS
  auto options = parse_args(argc, argv);
  if (!options) {
    // ERROR SI FALTA UN ARGUMENTO
    if (options.error() == parse_args_errors::missing_argument) {
      std::cerr << "Error: missing argument\n";
    }
    // ERROR SI NO CONOCE LA OPCIÓN
    else if (options.error() == parse_args_errors::unknown_option) {
      std::cerr << "Error: unknown option\n";
    }
    return EXIT_FAILURE;
  }
  // MENSAJE DE AYUDA
  if (options->show_help) {
    std::cout << "Usage: docserver [-v | --verbose] [-h | --help] ARCHIVO\n";
    return EXIT_SUCCESS;
  }
  // LEER ARCHIVO
  auto file = read_all(options->output_filename);
  // Si el archivo no se puede abrir porque no existe
  if (!file) {
    std::cerr << "404 Not Found\n";
    return EXIT_FAILURE;
  }
  // Si el archivo no se puede abrir porque no tiene permisos
  if (file.error() == EACCES) {
    std::cerr << "403 Forbidden\n";
    return EXIT_FAILURE;
  }
  else {
    // OPCIÓN -v | --verbose
    if (options->verbose) {
      std::cout << "open: abre el archivo " << options->output_filename << std::endl;
      std::cout << "read: lee " << file->get().size() << " bytes del archivo " << options->output_filename << std::endl;
    }
    std::string_view body = file->get();
    std::string header = "No size option\n";
    if (!options->no_size) {
      int size = body.size();
      std::ostringstream oss;
      oss << "Content-Length: " << size << "\n";
      header = oss.str();
    }
    send_response(header, body);
  }
  return 0;
}