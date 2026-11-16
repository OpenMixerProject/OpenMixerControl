#pragma once
#include "page.h"
using namespace std;

namespace OMC
{

class PageLibrary: public Page {
    public:
        PageLibrary(PageBaseParameter* pagebasepar) : Page(pagebasepar) {
            tabLayer0 = objects.maintab;
            tabIndex0 = 4;
        }
};

}