#include <gtest/gtest.h>
#include "headers.h"
#include <vector>
#include <string>


TEST(iterHeaders, EmptyHeaders) {
    std::string_view req = "";
    int call_count = 0;
    iterHeaders(req, [&call_count](std::string_view key, std::string_view value) {
        call_count++;
    });
    EXPECT_EQ(call_count, 0);
}


TEST(iterHeaders, SkipRequestLine) {
    // чо стартовая строка без : не крашит парсер.
    std::string_view req = "GET /index.html HTTP/1.1\r\nHost: example.com\r\n\r\n";
    std::vector<std::pair<std::string, std::string>> headers;
    
    iterHeaders(req, [&headers](std::string_view key, std::string_view value) {
        headers.emplace_back(std::string(key), std::string(value));
    });
    
    ASSERT_EQ(headers.size(), 1);
    EXPECT_EQ(headers[0].first, "Host");
    EXPECT_EQ(headers[0].second, "example.com");
}


TEST(iterHeaders, HeaderTrimSpaces) {
    std::string_view req = "Host:   example.com:8080   \r\n\r\n";
    std::vector<std::pair<std::string, std::string>> headers;
    iterHeaders(req, [&headers](std::string_view key, std::string_view value) {
        headers.emplace_back(std::string(key), std::string(value));
    });
    ASSERT_EQ(headers.size(), 1);
    EXPECT_EQ(headers[0].first, "Host");
    // Пробелы вокруг значения должны быть удалены (ltrim/rtrim)
    EXPECT_EQ(headers[0].second, "example.com:8080");
}

TEST(iterHeaders, MultipleHeaders) {
    std::string_view req = "Host: example.com\r\nContent-Length: 42\r\nConnection: close\r\n\r\n";
    std::vector<std::pair<std::string, std::string>> headers;
    iterHeaders(req, [&headers](std::string_view key, std::string_view value) {
        headers.emplace_back(std::string(key), std::string(value));
    });
    ASSERT_EQ(headers.size(), 3);
    EXPECT_EQ(headers[0].first, "Host");
    EXPECT_EQ(headers[0].second, "example.com");
    EXPECT_EQ(headers[1].first, "Content-Length");
    EXPECT_EQ(headers[1].second, "42");
    EXPECT_EQ(headers[2].first, "Connection");
    EXPECT_EQ(headers[2].second, "close");
}

TEST(iterHeaders, MultSameHeaders) {
    std::string_view req = "X-Custom: val1\r\nX-Custom: val2\r\n\r\n";
    std::vector<std::pair<std::string, std::string>> headers;
    iterHeaders(req, [&headers](std::string_view key, std::string_view value) {
        headers.emplace_back(std::string(key), std::string(value));
    });
    ASSERT_EQ(headers.size(), 2);
    EXPECT_EQ(headers[0].first, "X-Custom");
    EXPECT_EQ(headers[0].second, "val1");
    EXPECT_EQ(headers[1].first, "X-Custom");
    EXPECT_EQ(headers[1].second, "val2");
}

TEST(findHostPort, FindHostPort) {
    std::string_view req = "GET / HTTP/1.1\r\nHost: mysite.ru:8080\r\n\r\n";
    auto [host, port] = findHostPort(req);
    EXPECT_EQ(host, "mysite.ru");
    EXPECT_EQ(port, "8080");
    // порт не указан явным
    std::string_view req_default = "GET / HTTP/1.1\r\nHost: example.org\r\n\r\n";
    auto [host_def, port_def] = findHostPort(req_default);
    EXPECT_EQ(host_def, "example.org");
    EXPECT_EQ(port_def, "80");
}

// Host отсутствует
TEST(findHostPort, NoHost) {
    std::string_view req = "GET / HTTP/1.1\r\nContent-Length: 10\r\n\r\n";
    auto [host, port] = findHostPort(req);
    EXPECT_EQ(host, "");
    EXPECT_EQ(port, "80");
}

// Тест на успешный поиск Content-Length
TEST(findContentLength, Simple) {
    std::string_view rsp = "HTTP/1.1 200 OK\r\nContent-Length: 1024\r\n\r\n";
    auto len = findContentLength(rsp);
    ASSERT_TRUE(len.has_value());
    EXPECT_EQ(len.value(), 1024);
}

// Граничный случай: заголовок Content-Length отсутствует
TEST(findContentLength, NoContentLength) {
    std::string_view rsp = "HTTP/1.1 200 OK\r\nConnection: keep-alive\r\n\r\n";
    auto len = findContentLength(rsp);
    EXPECT_FALSE(len.has_value());
}

