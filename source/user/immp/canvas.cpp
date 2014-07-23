#include "canvas.h"

using namespace gui;

Canvas::Canvas() {
    panel = make_control<Panel>(
            Pos(0,0),
            Size(700, 500),
            make_layout<BorderLayout>());
}

void Canvas::set_button(std::shared_ptr<gui::ImageButton> newimg) {
    if (current_button) {
        panel->remove(current_button);
        current_button.reset();
    }
    current_button = newimg;
    panel->add(current_button, BorderLayout::CENTER);
}

