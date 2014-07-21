/**
 * $Id$
 * Copyright (C) 2008 - 2014 Nils Asmussen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "immp.h"
#include <gui/image/bitmapimage.h>
#include <gui/layout/borderlayout.h>
#include <gui/imagebutton.h>
#include <esc/proc.h>
#include <esc/thread.h>
#include <iostream>
#include <stdlib.h>

using namespace gui;
using namespace std;

void IMMP::create_splash() {
    splash_window = make_control<Window>("IMMP", Pos(200, 150));
	shared_ptr<Panel> root = splash_window->getRootPanel();
	root->getTheme().setPadding(0);
	root->setLayout(make_layout<BorderLayout>());
    shared_ptr<BitmapImage> logo_img =
        make_control<BitmapImage>("/etc/immp_logo.bmp");
    shared_ptr<ImageButton> logo =
        make_control<ImageButton>(logo_img);
	root->add(logo,BorderLayout::CENTER);
	splash_window->show(true);
}

void IMMP::create_mainwnd() {
    main_window = make_control<Window>("IMMP", Pos(50, 50));
	shared_ptr<Panel> root = main_window->getRootPanel();
	root->setLayout(make_layout<BorderLayout>());
    image_panel = make_control<Panel>(
            Pos(0,0), Size(700, 500), make_layout<BorderLayout>());
    root->add(image_panel, BorderLayout::CENTER);
    // TODO embed in Border control
    path_input = make_control<Editable>();
    root->add(path_input, BorderLayout::NORTH);
    // TODO embed in Border control
    current_file_label = make_control<Label>("current_file");
    root->add(current_file_label, BorderLayout::SOUTH);
    main_window->show(true);
}

void IMMP::state_machine() {
    while (immp_state > 0) {
        switch (immp_state) {
            case 1:
                // Initialisation
                create_splash();
                application->addWindow(splash_window);
                sleep(2000);
                application->removeWindow(splash_window);
                splash_window.reset();
                create_mainwnd();
                application->addWindow(main_window);
                // XXX should run event-driven from here
                return;

                immp_state++;
                break;
            default:
                immp_state = 0;
        }
    }
    application->exit();
}

int IMMP_state_machine_thread(void *arg) {
    IMMP *immp = reinterpret_cast<IMMP*>(arg);
    immp->state_machine();
    return 0;
}

int main() {
    Application *application = Application::create();
    IMMP immp(application);
	if(startthread(IMMP_state_machine_thread, &immp) < 0) {
		error("Unable to start state_machine thread");
    }
	return application->run();
}

