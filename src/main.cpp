#include "headers.h"

#include <boost/asio.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <string_view>
#include <iostream>
#include <print>
#include <string>
#include <utility>

using boost::asio::io_service;
using boost::asio::co_spawn;
using boost::asio::async_read_until;
using boost::asio::awaitable;
using boost::asio::use_awaitable;
using boost::system::error_code;
using boost::asio::buffer;
using boost::asio::dynamic_buffer;
using boost::asio::transfer_at_least;
using boost::asio::ip::tcp;

constexpr std::string_view delimiter = "\r\n\r\n";

// Сопрограмма прокси-сессии для одного клиента
awaitable<void> session(tcp::socket client_socket, io_service& io_service)
{
   
    try {
        std::string client_buffer;
        co_await boost::asio::async_read_until(
            client_socket, 
            boost::asio::dynamic_buffer(client_buffer), 
            delimiter, 
            use_awaitable);

        auto [host, port] = findHostPort(client_buffer);
        if (host.empty()) {
            std::println(stderr, "Error: Host header not found in client request.");
            co_return;
        }

        // 2. Установка асинхронного соединения с целевым сайтом
        tcp::resolver resolver(io_service);
        auto endpoints = 
            co_await resolver.async_resolve(host, port, use_awaitable);
        
        tcp::socket target_socket(io_service);
        co_await boost::asio::async_connect(target_socket, endpoints, use_awaitable);
        co_await boost::asio::async_write(target_socket, 
                                          boost::asio::buffer(client_buffer), 
                                          use_awaitable);

        // Чтение ответа от сайта
        std::string target_buffer;
        // заголовки ответа до разделителя \r\n\r\n
        size_t bytes_until_delim = co_await 
                boost::asio::async_read_until(target_socket, 
                    boost::asio::dynamic_buffer(target_buffer), 
                    delimiter, 
                    use_awaitable);

        //Content-Length для определения размера тела ответа
        auto content_length = 
                findContentLength(target_buffer);

        // Отправляем прочитанные заголовки ответа (и часть тела, если она уже попала в буфер) клиенту
        co_await boost::asio::async_write(client_socket, 
                                          boost::asio::buffer(target_buffer), 
                                          use_awaitable);

        // оставшегося тела ответа (есл есть)
        if (content_length.has_value() && content_length.value() > 0) {
            //сколько байт уже успели прочитать?
            size_t body_bytes_already_read = target_buffer.size() - bytes_until_delim;
            
            if (body_bytes_already_read < content_length.value()) {
                size_t remaining_bytes = content_length.value() - body_bytes_already_read;
                
                // Выделляем буф-р под оставшуюся часть тела
                std::string body_buffer;
                body_buffer.resize(remaining_bytes);
                
                // Читам сколко осталось
                co_await boost::asio::async_read(target_socket, 
                                                 boost::asio::buffer(body_buffer), 
                                                 boost::asio::transfer_exactly(remaining_bytes), 
                                                 use_awaitable);
                
                // Пересылаем лиенту
                co_await boost::asio::async_write(client_socket, 
                                                  boost::asio::buffer(body_buffer), 
                                                  use_awaitable);
            }
        } else if (!content_length.has_value()) {
            // Content-Length отсутствует (например, Transfer-Encoding: chunked 
            // или Connection: close).
            // Будем читать данные порциями, пока сайт не закроет соединение.
            std::string body_buffer;
            while (true) {
                body_buffer.clear();
                // Читаем порцию данных (минимум 1 байт)
                size_t bytes_read = co_await boost::asio::async_read
                        (target_socket, 
                         boost::asio::dynamic_buffer(body_buffer), 
                         boost::asio::transfer_at_least(1), 
                         use_awaitable
                        );
                
                co_await boost::asio::async_write(
                        client_socket, 
                        boost::asio::buffer(body_buffer), 
                        use_awaitable);
            }
        }

    } catch (const boost::system::system_error& e) {
        if (e.code() != boost::asio::error::eof) {
            std::println(stderr, "Session system error: {}", e.what());
        }
    } catch (const std::exception& e) {
        std::println(stderr, "Session exception: {}", e.what());
    }

    // закроем сокеты ввно ( ТЗ)
    error_code ec;
    client_socket.close(ec);
}

class Server
{
public:
    Server(io_service& io_service, short port)
      : io_service_(io_service)
      , acceptor_(io_service, 
            tcp::endpoint(tcp::v4(), port))
      , socket_(io_service)
    {
        do_accept();
    }

private:
    void do_accept()
    {
        // Принимаем входящее соединение
        acceptor_.async_accept(socket_,
            [this](error_code ec)
            {
                if (!ec) {
                    // Запускаем session на каждом уст-ом соединении co_spawn'ом
                    // std::move(socket_), xnj, передать владение сокетом корутине
                    boost::asio::co_spawn(
                        io_service_, 
                        session(std::move(socket_), io_service_), 
                        boost::asio::detached);
                } else {
                    std::println(stderr, "Accept error: {}", ec.message());
                }
                //для следующего подключения
                socket_ = tcp::socket(io_service_);
                do_accept();
            }
        );
    }

    io_service& io_service_;
    tcp::acceptor acceptor_;
    tcp::socket socket_;
};

int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            std::println(stderr, "Usage: proxy_server <listen_port>");
            return 1;
        }
        
        io_service io_service(1);
        Server server(io_service, std::atoi(argv[1]));
        io_service.run();

    } catch (const std::exception& e) {
        std::println(stderr, "Exception: {}", e.what());
    }
    return 0;
}
