#include <string>
#include <iostream>
#include <cassert>
#include <regex>

// Adapted from: https://gist.github.com/arthurafarias/56fec2cd49a32f374c02d1df2b6c350f

const std::regex PERCENT_ENCODING("%[0-9A-F]{2}");

std::string uri_decode(std::string uri) {
    std::string output = "";

    size_t n = uri.size();

    if (n < 3) {
        return uri;
    }

    // whether or not this loop instance translated something
    bool translated = false;
    for (int i = 0; i < n - 2; i++) {
        translated = false;
        std::string next_3_chars = uri.substr(i, 3);
        if (std::regex_match(next_3_chars, PERCENT_ENCODING)) {
            // replace % symbol with 0x to convert from hex to int
            next_3_chars.replace(0, 1, "0x");
            char next = static_cast<char>(std::stoi(next_3_chars, nullptr, 16));
            output.push_back(next);
            i += 2;
            translated = true;
        } else {
            output.push_back(uri[i]);
        }
    }

    if (!translated) {
        output += uri.substr(n - 2);
    }

    return output;
}


int main() {
    assert(uri_decode("hello") == "hello");
    assert(uri_decode("hi%20there%2520bruh") == "hi there%20bruh");
    assert(uri_decode("jnush%28") == "jnush(");
    assert(uri_decode("%2Afor%29go%32osh") == "*for)go2osh");
    assert(uri_decode("/") == "/");
    assert(uri_decode("index.html") == "index.html");
    assert(uri_decode("/index.html") == "/index.html");
}