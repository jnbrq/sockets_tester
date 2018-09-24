#ifndef CLIENT_HPP_INCLUDED
#define CLIENT_HPP_INCLUDED

#include <boost/asio.hpp>
#include <type_traits>
#include <functional>
#include <string>
#include <thread>
#include <string_view>
#include <memory>
#include <iostream>
#include <sstream>
#include <queue>

namespace client {
    namespace detail {
        using namespace boost::asio;
        using boost::system::error_code;

        struct client: std::enable_shared_from_this<client> {
            client(
                std::function<void(std::string const &)> fn_connect,
                std::function<void(std::string const &)> fn_receive,
                std::function<void(std::string const &)> fn_error) :
                socket{ ctx },
                resolver{ ctx },
                fn_connect{ std::move(fn_connect) },
                fn_receive{ std::move(fn_receive) },
                fn_error{ std::move(fn_error) } {
            }

            // thread safe
            void start(std::string_view address, std::string_view port) {
                post(ctx, [this, address{ std::string{address} }, port{ std::string{port} }]() {
                    async_connect(address, port);
                });

                std::thread t{ [this, ref = shared_from_this()] {
                    std::cerr << "[" << this << "] thread started\n";
                    // runs as long as there is a job to do
                    ctx.run();
                    std::cerr << "[" << this << "] thread completed\n";
                } };
                t.detach();
            }

            // thread safe
            void stop() {
                ctx.stop();
            }

            // thread safe
            void send(std::string_view data) {
                post(ctx, [this, data{ std::string{data} + "\n" }]() {
                    enqueue_send(std::move(data));
                });
            }
        private:
            io_context ctx;
            ip::tcp::socket socket;
            ip::tcp::resolver resolver;

            std::function<void(const std::string &)> fn_receive;
            std::function<void(std::string const &)> fn_connect;
            std::function<void(const std::string &)> fn_error;

            std::queue<std::string> write_queue;
            bool writing{ false };
            bool connected{ false };

            boost::asio::streambuf buf;

            void error(error_code const &ec, const std::string &desc) {
                if (ec != error::operation_aborted) {
                    std::ostringstream oss;
                    oss << desc << " message() = " << ec.message();
                    auto msg = oss.str();
                    if (fn_error) fn_error(msg);
                    std::cerr << "[" << this << "] " << msg << "\n";
                }
                else {
                    std::cerr << "[" << this << "] " << desc << " operation aborted\n";
                }
            }

            void async_read() {
                async_read_until(socket, buf, '\n', [this](error_code const &ec, std::size_t) {
                    if (ec) {
                        error(ec, "Read error!");
                        return;
                    }
                    std::istream is{ &buf };
                    std::string line;
                    std::getline(is, line);
                    if (fn_receive) fn_receive(line);
                    std::cerr << "[" << this << "] received: " << line << "\n";
                    async_read();
                });
            }

            void async_connect(const std::string &address, const std::string &port) {
                resolver.async_resolve(address, port, [this](error_code const &ec, ip::tcp::resolver::results_type const &endpoints) {
                    if (ec) {
                        error(ec, "Resolver error!");
                        return;
                    }
                    boost::asio::async_connect(socket, endpoints, [this](error_code const &ec, ip::tcp::endpoint const &ep) {
                        if (ec) {
                            error(ec, "Connect error!");
                            return;
                        }
                        connected = true;
                        async_write_one();  // in case if it's already full
                        async_read();
                        if (fn_connect) fn_connect(ep.address().to_string());
                        std::cerr << "[" << this << "] connected to: " << ep.address().to_string() << "\n";
                    });
                });
            }

            void async_write_one() {
                if (write_queue.empty() || !connected) {
                    writing = false;
                    return;
                }
                if (!writing) {
                    async_write(socket, buffer(write_queue.front()), [this](error_code const &ec, std::size_t) {
                        if (ec) {
                            error(ec, "Write error!");
                            return;
                        }

                        write_queue.pop();
                        async_write_one();
                    });
                    writing = true;
                }
            }

            void enqueue_send(std::string data) {
                write_queue.push(std::move(data));
                async_write_one();
            }
        };
    }

    using detail::client;
}

#endif // CLIENT_HPP_INCLUDED
