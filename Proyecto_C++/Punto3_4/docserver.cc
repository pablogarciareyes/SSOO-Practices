/**
 * Universidad de La Laguna
 * Escuela Superior de Ingeniería y Tecnología
 * Asignatura: Sistemas Operativos (SSOO)
 * Curso: 2º
 * Proyecto C++: Servidor de Documentos
 * @author 
 * @file docserver.cc
 * @brief docserver [-v | --verbose] [-h | --help] [-p | --port] [-b | --base]
 * @bug No hay bugs conocidos
 *     
 * Compilar con: g++ -std=c++23 docserver.cc Functions.cc
 * Ejecutar: ./a.out -b /home/usuario/Proyecto_C++/Punto3_4
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
        } else if (options.error() == parse_args_errors::invalid_route) {
            std::cerr << "Error: la ruta base no existe\n";
        }
        return EXIT_FAILURE;
    }

    // Mostrar ayuda si es necesario
    if (options->show_help) {
        std::cout << "Uso: docserver [-v | --verbose] [-h | --help] [-p | --port] [-b | --base]\n";
        return EXIT_SUCCESS;
    }

    // Crear un socket y asignarle el puerto indicado
    uint16_t port = options->port ? options->port_value : 8080; // Puerto por defecto: 8080
    auto sock_fd = make_socket(port);

    if (!sock_fd) {
        std::cerr << "Error al crear el socket\n";
        return EXIT_FAILURE;
    }

    if (options->verbose) {
        std::cout << "Socket creado en el puerto " << port << '\n';
    }

    // Poner el socket a la escucha
    int result = listen_connection(sock_fd.value());
    if (result < 0) {
        std::cerr << "Error al poner el socket a la escucha\n";
        return EXIT_FAILURE;
    }

    if (options->verbose) {
        std::cout << "Escuchando en el puerto " << port << '\n';
    }

    // Bucle principal para manejar conexiones
    while (true) {
        sockaddr_in client_addr{};
        auto new_fd = accept_connection(sock_fd.value(), client_addr);

        if (!new_fd) {
            std::cerr << "Error al aceptar la conexión\n";
            continue; // Continuar aceptando nuevas conexiones
        }

        // Leer la solicitud del cliente
        auto request = receive_request(new_fd.value(), 4096);
        if (!request) {
            if (request.error() == ECONNRESET) {
                std::cerr << "Error: la conexión fue restablecida por el cliente\n";
            } else {
                std::cerr << "Error fatal al leer la solicitud\n";
            }
            close(new_fd.value().get());
            continue;
        }

        // Procesar la solicitud para extraer la ruta del archivo
        std::istringstream iss(request.value());
        std::string get, file_path;
        iss >> get >> file_path;

        // Comprobar que la solicitud es válida
        if (get != "GET" || file_path.empty() || file_path[0] != '/') {
            std::string error_response = "400 Bad Request\n";
            send_response(new_fd.value(), "Error ", error_response);
            close(new_fd.value().get());
            continue;
        }

        // Ajustar la ruta del archivo como relativa al directorio base
        std::string path_option;
        if (!options->base) {
            path_option = getcwd(nullptr, 0); // En caso de no haber ruta especificada se toma la del directorio actual del archivo
        } else {
            path_option = options->ruta_base; // En caso de haber ruta especificada se toma la ruta base
        }
        // Comprobar que la ruta base exista mediante access()
        if (access(path_option.c_str(), F_OK) == -1) {
            std::cerr << "Error: la ruta base no existe\n";
            send_response(new_fd.value(), "Error", "404 Not Found\n");
            close(new_fd.value().get());
            continue;
        }
        std::string complete_path = path_option + file_path; // Se concatena la ruta base con la ruta del archivo solicitado
        if (options->verbose) {
            std::cout << "Solicitud de archivo: " << complete_path << '\n';
        }

        // -------------------
        //       PUNTO 4
        // -------------------

        // Verificar si la ruta empieza con /bin/ para ejecutar un programa
        if (file_path.rfind("/bin/", 0) == 0) {
            // Ajustamos las variables de entorno 
            exec_environment env;
            env.REQUEST_PATH = complete_path;
            env.SERVER_BASEDIR = path_option;
            env.REMOTE_PORT = std::to_string(ntohs(client_addr.sin_port));
            env.REMOTE_IP = inet_ntoa(client_addr.sin_addr);

            // Ejecutar el programa
            auto result = execute_program(complete_path, env);
            // Comprobación de errores
            if (!result) { 
                const auto& error = result.error();
                if (error.error_code == ENOENT) {
                    std::cerr << "Error: el programa no existe (ENOENT)\n";
                    send_response(new_fd.value(), "Error", "404 Not Found\n");
                } else if (error.error_code == EACCES) {
                    std::cerr << "Error: no se tienen permisos para ejecutar el programa (EACCES)\n";
                    send_response(new_fd.value(), "Error", "403 Forbidden\n");
                } else {
                    std::cerr << "El programa terminó con un código de error: " << error.exit_code << "\n";
                    std::string response = "500 Internal Server Error\n";
                    send_response(new_fd.value(), "Content-Length: " + std::to_string(response.size()) + "\n\n", response);
                }
                close(new_fd.value().get());
                continue;
            }
            // Responder con el contenido del archivo
            std::string_view body = result.value();
            std::ostringstream oss;
            oss << "Content-Length: " << body.size() << "\n\n";
            std::string header = oss.str();
            // Enviar la respuesta al cliente
            int send_result = send_response(new_fd.value(), header, body);
            if (send_result < 0 && errno == ECONNRESET) {
                std::cerr << "Error: la conexión fue restablecida por el cliente\n";
            } else if (send_result < 0) {
                std::cerr << "Error fatal al enviar la respuesta\n";
                close(new_fd.value().get());
                return EXIT_FAILURE;
            }

            if (options->verbose) {
                std::cout << "Respuesta enviada con " << body.size() << " bytes\n";
            }
        }
        else {
            // Ruta no comienza con /bin/, leer archivo
            auto file = read_all(complete_path);
            if (!file) {
                if (file.error() == ENOENT) {
                    send_response(new_fd.value(), "Error", "404 Not Found\n");
                } else if (file.error() == EACCES) {
                    send_response(new_fd.value(), "Error", "403 Forbidden\n");
                } else {
                    std::cerr << "Error fatal al leer el archivo\n";
                }
                close(new_fd.value().get());
                continue;
            }

            // ------------------------------------------------------------------------------------------------------------------------------------------------------
            // MODIFICACIÓN, cabe añadir que esto ya lo habia hecho para que saliese con la opción -v | --verbose, simplemente lo he adaptado para que
            // salga por el socat (cliente) en lugar de la terminal donde se ejecuta (servidor).
            // std::ostringstream response_stream;
            // response_stream << "IP: " << inet_ntoa(client_addr.sin_addr) << "\nPUERTO " << ntohs(client_addr.sin_port) << "\n";
            // std::string body_mod = response_stream.str();
            // std::ostringstream header_stream;
            // header_stream << "\n" << "Content-Length: " << body_mod.size() << "\n\n";
            // std::string header_mod = header_stream.str();
            // send_response(new_fd.value(), header_mod, body_mod);
            // ------------------------------------------------------------------------------------------------------------------------------------------------------

            // Responder con el contenido del archivo
            std::string_view body = file->get();
            std::ostringstream oss;
            oss << "Content-Length: " << body.size() << "\n\n";
            std::string header = oss.str();
            // Enviar la respuesta al cliente
            int send_result = send_response(new_fd.value(), header, body);
            if (send_result < 0 && errno == ECONNRESET) {
                std::cerr << "Error: la conexión fue restablecida por el cliente\n";
            } else if (send_result < 0) {
                std::cerr << "Error fatal al enviar la respuesta\n";
                close(new_fd.value().get());
                return EXIT_FAILURE;
            }
            if (options->verbose) {
                std::cout << "Respuesta enviada con " << body.size() << " bytes\n";
            }
        }
        // Cerrar la conexión con el cliente
        close(new_fd.value().get());
        if (options->verbose) {
            std::cout << "Conexión cerrada\n";
        }
    }
    // Cerrar el socket del servidor
    close(sock_fd.value().get());
    if (options->verbose) {
        std::cout << "Socket cerrado\n";
    }
    return EXIT_SUCCESS;
}
