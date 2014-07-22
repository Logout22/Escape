#include "canvas.h"

using namespace gui;

Canvas::Canvas() {
    panel = make_control<Panel>(
            Pos(0,0),
            Size(700, 500),
            make_layout<BorderLayout>());
}

