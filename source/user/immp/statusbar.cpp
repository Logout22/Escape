#include "statusbar.h"
#include <gui/layout/flowlayout.h>

using namespace gui;

StatusBar::StatusBar() {
    status_panel = make_control<Panel>(
            make_layout<FlowLayout>(Align(FRONT), false));
    current_file_label = make_control<Label>("current_file");
    status_panel->add(current_file_label);
}

