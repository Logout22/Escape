#pragma once

#include <esc/common.h>
#include <gui/application.h>
#include <gui/window.h>
#include <gui/editable.h>
#include "canvas.h"
#include "statusbar.h"

class IMMP {
    private:
        gui::Application *application;
        std::shared_ptr<gui::Window> splash_window;
        std::shared_ptr<gui::Window> main_window;
        std::unique_ptr<Canvas> canvas;
        std::shared_ptr<gui::Editable> path_input;
        std::unique_ptr<StatusBar> status_bar;
        uint32_t immp_state;

        void create_splash();
        void create_mainwnd();
    public:
        IMMP(gui::Application *app) : application(app), immp_state(1) {}
        void state_machine();
};

// Helper function
int IMMP_state_machine_thread(void *arg);

