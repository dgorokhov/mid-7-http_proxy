#include "headers.h"

#include <ranges>
#include <string_view>
#include <algorithm>
#include <charconv>
#include <cctype>

using namespace std::string_view_literals;

using Callback = std::function<void(std::string_view, std::string_view)>;

void iterHeaders(std::string_view req, Callback&& callback) {
    // Разделяем запрос на строки по \r\n с помощью C++23 views::split
    auto lines = req | std::views::split("\r\n"sv);
    
    for (auto&& line_range : lines) {
        // Создаем string_view из диапазона (диапазон C++23 split возвращает subrange)
        std::string_view line(line_range.begin(), line_range.end());
        
        if (line.empty()) {
            continue;
        }
        
        // Ищем двоеточие — разделитель ключа и значения заголовка
        auto colon_pos = line.find(':');
        if (colon_pos != std::string_view::npos) {
            std::string_view key = line.substr(0, colon_pos);
            std::string_view value = line.substr(colon_pos + 1);
            
            // Удаляем пробелы в начале значения (ltrim)
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
                value.remove_prefix(1);
            }
            // Удаляем пробелы в конце значения (rtrim)
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
                value.remove_suffix(1);
            }
            
            callback(key, value);
        }
    }
}

std::pair<std::string, std::string> findHostPort(std::string_view req) {
    std::string host = "";
    std::string port = "80"; // Значение по умолчанию для HTTP

    iterHeaders(req, [&host, &port](std::string_view key, std::string_view value) {
        // Проверяем заголовок Host (сравниваем с учетом регистра и без)
        if (key == "Host" || key == "host") {
            auto colon_pos = value.find(':');
            if (colon_pos != std::string_view::npos) {
                host = std::string(value.substr(0, colon_pos));
                port = std::string(value.substr(colon_pos + 1));
            } else {
                host = std::string(value);
                port = "80";
            }
        }
    });

    return {host, port};
}

std::optional<size_t> findContentLength(std::string_view rsp) {
    std::optional<size_t> content_length = std::nullopt;

    iterHeaders(rsp, [&content_length](std::string_view key, std::string_view value) {
        if (key == "Content-Length" || key == "content-length") {
            size_t val = 0;
            // Безопасный и быстрый парсинг строки в число без аллокаций
            auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), val);
            if (ec == std::errc()) {
                content_length = val;
            }
        }
    });

    return content_length;
}
