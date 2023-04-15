#ifndef DEFAULTS_H
#define DEFAULTS_H

#include <core/Config/Config.h>


// TODO: move outside "/core"?
namespace Defaults
{
    namespace
    {
        ConfigDB m_windowDatabase;
        ConfigDB m_loggingDatabase;
    }
    
    // window configuration.
    struct windowConfig {
        uint32_t MAIN_WINDOW_WIDTH;  // width of the main displayed window.
        uint32_t MAIN_WINDOW_HEIGHT;  // height of the main displayed window.
        std::string MAIN_WINDOW_NAME;  // name of the main displayed window.
    };
    extern windowConfig windowDefaults;  // default/read window configuration.

    // logging configuration.

    // misc. configuration.
    struct miscConfig {
        std::string SALAMANDER_ROOT_DIRECTORY;  // the "root" directory of the project(ex: /home/lucas/salamander/), read from the %SALAMANDER_ROOT% environment variable.
    };
    extern miscConfig miscDefaults;  // default/read misc. configuration.

    // initialize all of the defaults.
    void initializeDefaults();
}


#endif  // DEFAULTS_H
