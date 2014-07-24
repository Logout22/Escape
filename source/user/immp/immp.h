#pragma once

#include "global_defs.h"
#include <gui/application.h>
#include <gui/window.h>
#include <gui/editable.h>
#include "canvas.h"
#include "directory.h"
#include "statusbar.h"

class IMMP {
    private:
        gui::Application *application;
        std::shared_ptr<gui::Window> splash_window;
        std::shared_ptr<gui::Window> main_window;
        std::unique_ptr<Canvas> canvas;
        std::shared_ptr<gui::Editable> path_input;
        std::shared_ptr<StatusBar> status_bar;
        uint32_t immp_state;
        std::unique_ptr<Directory> current_dir;

        void create_splash();
        void create_mainwnd();
        void update_canvas();
    public:
        IMMP(gui::Application *app) : application(app), immp_state(1) {}
        void state_machine();
        void change_dir(gui::UIElement&)
        { current_dir->change_path(path_input->getText()); update_canvas(); }
        void select_prev_image(gui::UIElement&)
        { current_dir->select_prev(); update_canvas(); }
        void select_next_image(gui::UIElement&)
        { current_dir->select_next(); update_canvas(); }
};

// Helper function
int IMMP_state_machine_thread(void *arg);

