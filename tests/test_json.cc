#include <util/file.hpp>
#include <util/io.hpp>
#include <rapidjson/document.h>
#include <rapidjson/reader.h>
#include <iostream>

int main(void)
{
    using namespace rapidjson;
    Document json_document;
    std::string file_buffer = util::read_file("data/test.json");

    json_document.Parse(file_buffer.c_str());

    if (!json_document.IsObject()) {
        std::cerr << "Invalid document" << std::endl;
        return 1;
    }

    if (json_document.HasMember("id") && json_document["id"].IsString()) {
        std::cout << json_document["id"].GetString() << std::endl;
    }

    if (json_document.HasMember("value") && json_document["value"].IsNumber()) {
        std::cout << json_document["value"].GetInt() << std::endl;
    }
}

