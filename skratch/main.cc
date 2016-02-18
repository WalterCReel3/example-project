#include <iostream>
#include <fstream>
// #include <unistd.h>
#include "application.hpp"

using namespace std;

Application *g_application = 0;
ofstream logging;

int main(int, char**)
{
    logging.open("runlog.txt");
    try {
        Application app;
        g_application = &app;
        app.game_loop();
    } catch (exception &e) {
        logging << e.what() << endl;
    }
    logging.close();

    return 0;
}

// vim: set sts=2 sw=2 expandtab:
