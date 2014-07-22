#pragma once

#include <esc/common.h>
#include <gui/label.h>
#include <string>

class StatusBar {
    private:
        std::shared_ptr<gui::Label> current_file_label;
        std::string current_file_name;
    public:
        // XXX
        StatusBar() :
            current_file_label(gui::make_control<gui::Label>("current_file"))
            {}
        std::shared_ptr<gui::Label> get_control()
            { return current_file_label; }
};

