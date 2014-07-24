#pragma once

#include "global_defs.h"
#include <gui/label.h>
#include <gui/panel.h>
#include <string>

class StatusBar {
    private:
        std::shared_ptr<gui::Label> current_file_label;
        std::shared_ptr<gui::Panel> status_panel;
        std::string current_file_name;
    public:
        StatusBar();
        void set_filename(std::string fname)
            { current_file_label->setText(fname); }
        std::shared_ptr<gui::Panel> get_control() { return status_panel; }
};

