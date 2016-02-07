#include <loaders/sparrow.hpp>
#include <gfx/spritesheet.hpp>
// #include <util/xml.hpp>
// #include <format.h>
#include <stdexcept>

namespace loaders
{

// void load_subtextures(gfx::Spritesheet& spritesheet,
//                       util::simplexml::Element element)
// {
//     for (auto elem : element.children()) {
//         int x = elem.get_attribute_int("x");
//         int y = elem.get_attribute_int("y");
//         int w = elem.get_attribute_int("width");
//         int h = elem.get_attribute_int("height");
//         std::string name = elem.get_attribute("name");
//         gfx::SpritesheetFrame frame(name, x, y, 0, 0, w, h);
//         spritesheet.add_frame(frame);
//     }
// }
// 
// void load_sparrow(gfx::Spritesheet& spritesheet, const char * path)
// {
//     std::string file_buffer = util::read_file(path);
//     util::simplexml::Document document(file_buffer);
//     util::simplexml::Element root = document.document_element();
//     load_subtextures(spritesheet, root);
// }

} // namespace loaders

