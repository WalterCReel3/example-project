#ifndef __GFX_SYSTEM_HPP__
#define __GFX_SYSTEM_HPP__

#include <string>
#include <vector>
#include <gfx/context.hpp>
#include <util/nocopy.hpp>

namespace gfx
{

class System
{
private:
    System();

public:
    DISALLOW_COPY_AND_ASSIGN(System);

    ~System();
    Context* create_context(const std::string& name);

private:
    std::vector<Context*> _contexts;

    // Static Members
    static System* _instance;

public:
    static System* get_instance();
};

} // namespace gfx

#endif
