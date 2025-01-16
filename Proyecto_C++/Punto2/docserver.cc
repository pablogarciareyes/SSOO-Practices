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
 *     
 * Compilar con: g++ -std=c++23 docserver.cc Functions.cc
 * socat STDIO TCP:127.0.0.1:8080
*/

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
#include "Functions.h"

int main(int argc, char* argv[]) {
    // Procesar los argumentos de la línea de comandos
    auto options = parse_args(argc, argv);
    if (!options) {
        if (options.error() == parse_args_errors::missing_argument) {
            std::cerr << "Error: falta un argumento\n";
        } else if (options.error() == parse_args_errors::unknown_option) {
            std::cerr << "Error: opción desconocida\n";
        }
        return EXIT_FAILURE;
    }

    // Mostrar ayuda si es necesario
    if (options->show_help) {
        std::cout << "Uso: docserver [-v | --verbose] [-h | --help] [-p | --port] ARCHIVO\n";
        return EXIT_SUCCESS;
    }

    // Verificar que se ha indicado un nombre de archivo
    if (options->output_filename.empty()) {
        std::cerr << "Error: no se ha indicado un nombre de archivo.\n";
        return EXIT_FAILURE;
    }

    // Crear un socket y asignarle el puerto indicado
    uint16_t port = options->port ? 8080 : 8080; // Puerto por defecto o el indicado
    auto sock_fd = make_socket(port);

    // -----------------------------
    // ARREGLAR COMPROBACIÓN DE SOCKET
    if (!sock_fd) {
        std::cerr << "Error al crear el socket\n";
        return EXIT_FAILURE;
    }
    // -----------------------------
    
    // OPCIÓN -v | --verbose
    if (options->verbose) {
        std::cout << "Socket creado en el puerto " << port << '\n';
    }

    // Poner el socket a la escucha
    int result = listen_connection(sock_fd.value());
    if (result < 0) {
        std::cerr << "Error al poner el socket a la escucha\n";
        return EXIT_FAILURE;
    }
    // OPCIÓN -v | --verbose
    if (options->verbose) {
        std::cout << "Escuchando en el puerto " << port << '\n';
    }

    // Bucle para aceptar conexiones
    while (true) {
        sockaddr_in client_addr{};
        // Aceptar una conexión
        auto new_fd = accept_connection(sock_fd.value(), client_addr);
        if (!new_fd) {
            std::cerr << "Error al aceptar la conexión\n";
            continue; // No terminamos el servidor, seguimos intentando aceptar conexiones
        }
        // OPCIÓN -v | --verbose
        if (options->verbose) {
            std::cout << "Conexión aceptada\n";
        }
        // Leer archivo en memoria
        auto file = read_all(options->output_filename);
        if (!file) {
            if (file.error() == ENOENT) {
                // Error: archivo no encontrado
                std::cerr << "404 Not Found\n";
                send_response(new_fd.value(), "Error: ", "404 Not Found\n");
                return EXIT_FAILURE;
            } else if (file.error() == EACCES) {
                // Error: permisos
                std::cerr << "403 Forbidden\n";
                send_response(new_fd.value(), "Error", "403 Forbidden\n");
                return EXIT_FAILURE;
            } else {
                // Otro error
                std::cerr << "Error fatal al leer el archivo\n";
                return EXIT_FAILURE; // Terminar el servidor en caso de otro error grave
            }
            // OPCIÓN -v | --verbose
            if (options->verbose) {
                std::cout << "open: abre el archivo " << options->output_filename << '\n';
                std::cout << "read: lee " << file->get().size() << " bytes del archivo " << options->output_filename << '\n';
            }
        } else {
            // Enviar el contenido del archivo
            std::string_view body = file->get();
            int size = body.size();
            std::ostringstream oss;
            oss << "Content-Length: " << size << "\n";
            std::string header = oss.str();
            int send_result = send_response(new_fd.value(), header, body);
            if (send_result < 0) {
                if (errno == ECONNRESET) {
                    std::cerr << "Error: la conexión fue restablecida por el cliente\n";
                    // No cerramos la conexión, seguimos aceptando conexiones
                    continue;
                } else {
                    std::cerr << "Error al enviar la respuesta\n";
                    return EXIT_FAILURE; // Error fatal
                }
            }
            // OPCIÓN -v | --verbose
            if (options->verbose) {
                std::cout << "send: envía " << size << " bytes al cliente\n";
            }
        }
        // Cerrar la conexión con el cliente
        close(new_fd.value().get());
        // OPCIÓN -v | --verbose
        if (options->verbose) {
            std::cout << "close: cierra la conexión\n";
        }
    }
    // Cerrar el socket del servidor
    close(sock_fd.value().get());
    // OPCIÓN -v | --verbose
    if (options->verbose) {
        std::cout << "close: cierra el socket\n";
    }
    return 0;
}