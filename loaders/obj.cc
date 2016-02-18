#include <util/io.hpp>
#include <util/file.hpp>
#include <util/string.hpp>
#include <loaders/obj.hpp>
#include <cstdio>
#include <vector>
#include <string>
#include <stdexcept>

namespace loaders
{

void parse_vertex(const std::vector<std::string> & tokens,
                  gfx::ObjModel& model)
{
    if (tokens.size() < 4) {
        throw std::runtime_error("Loader error invalid line");
    }
    float x = ::strtod(tokens[1].c_str(), NULL);
    float y = ::strtod(tokens[2].c_str(), NULL);
    float z = ::strtod(tokens[3].c_str(), NULL);
    // FIXME - Faking out a color here.
    model.add_vertex(math::Vector3(x, y, z),
                     math::Vector3(1.0-x, 1.0-y, 1.0-z));
}

void parse_face(const std::vector<std::string> & tokens,
                gfx::ObjModel& model)
{
    if (tokens.size() < 4) {
        throw std::runtime_error("Loader error invalid line");
    }
    long v1 = ::strtol(tokens[1].c_str(), NULL, 10)-1;
    long v2 = ::strtol(tokens[2].c_str(), NULL, 10)-1;
    long v3 = ::strtol(tokens[3].c_str(), NULL, 10)-1;
    model.add_face(v1, v2, v3);
}

void load_obj_model(const std::string& filename, gfx::ObjModel& model)
{
    using namespace std;
    using namespace util;
    typedef quoted_whitespace_tokenizer<string> tokenizer_t;
    typedef token_generator<string::const_iterator, tokenizer_t> token_gen_t;

    std::string file_buffer;
    util::File input(filename, util::OpenReadOnly);
    util::read_all(input, file_buffer);
    line_iterator<string> li(file_buffer);
    line_iterator<string> eos;
    vector<string> tokens;

    for ( ; li != eos; ++li) {
        string token;
        token_gen_t gentok(li->first, li->second, tokenizer_t());
        for (token=gentok(); token.size() != 0; token=gentok()) {
            tokens.push_back(token);
        }
        if (tokens.size() > 0) {
            const string & keyword = tokens[0];
            if (keyword == "v") {
                parse_vertex(tokens, model);
            } else if (keyword == "f") {
                parse_face(tokens, model);
            }
        }
        tokens.clear();
    }
}

} // namespace loaders

// vim: sts=2 sw=2 expandtab:
