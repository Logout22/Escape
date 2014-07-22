#pragma once

#include <esc/common.h>
#include <gui/panel.h>
#include <gui/imagebutton.h>

class Canvas {
    private:
        std::shared_ptr<gui::Panel> panel;
        std::shared_ptr<gui::ImageButton> current_button;
    public:
        Canvas() {}
};

