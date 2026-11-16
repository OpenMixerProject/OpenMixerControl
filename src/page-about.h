#pragma once
#include "page.h"
using namespace std;

namespace OMC
{

class PageAbout: public Page {
    public:
        PageAbout(PageBaseParameter* pagebasepar) : Page(pagebasepar) {
            prevPage = X32_PAGE::SETUP_MIXER_CONFIG;
            nextPage = X32_PAGE::DEBUG;
            tabLayer0 = objects.maintab;
            tabIndex0 = 3;
            tabLayer1 = objects.setuptab;
            tabIndex1 = 4;
            hideEncoders = true;
        }
};

}
