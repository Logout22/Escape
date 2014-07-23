#pragma once

#include <esc/common.h>
#include <gui/panel.h>
#include <gui/imagebutton.h>
#include <gui/layout/borderlayout.h>

class Canvas {
    private:
        std::shared_ptr<gui::Panel> panel;
        std::shared_ptr<gui::ImageButton> current_button;
    public:
        Canvas();
        std::shared_ptr<gui::Panel> get_control() { return panel; }
        void set_button(std::shared_ptr<gui::ImageButton> newimg);
};

