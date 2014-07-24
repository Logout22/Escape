#include "directory.h"
#include <iostream>

using namespace std;
using namespace esc;

bool Directory::change_path(const string &path) {
    try {
        auto new_dir = unique_ptr<file>(new file(path));
        if (!new_dir->is_dir()) return false;
        image_files.clear();
        // by default do not list hidden files:
        // XXX handle all file endings
        for (auto const &dire : new_dir->list_files(false, "*.bmp")) {
            image_files.push_back(shared_ptr<ImageFile>(
                        new ImageFile(path, dire.d_name)));
        }
        dirhandle = unique_ptr<file>(new file(*new_dir));
        if (image_files.size() > 0) {
            current_image = image_files.begin();
            load_image(*current_image);
        } else {
            current_image = image_files.end();
            update_statusbar("");
        }
    } catch(const default_error& e) {
        cerr << e.what() << endl;
        return false;
	}
    return true;
}

void Directory::load_image(shared_ptr<ImageFile> file_to_load) {
    file_to_load->load_image();
    update_statusbar(file_to_load->get_filename());
}

bool Directory::select_image(ImageFileIterator new_image) {
    // check if there are images at all:
    if (current_image == image_files.end()) return false;
    (*current_image)->unload_image();
    load_image(*new_image);
    current_image = new_image;
    return true;
}

void Directory::update_statusbar(const string &fname) {
    statbar->set_filename(fname);
}

shared_ptr<ImageFile> Directory::get_current_image() {
    if (current_image == image_files.end()) {
        return shared_ptr<ImageFile>();
    } else {
        return *current_image;
    }
}

bool Directory::select_prev() {
    if (current_image == image_files.begin()) return false;
    return select_image(current_image - 1);
}

bool Directory::select_next() {
    auto new_image = current_image + 1;
    if (new_image == image_files.end()) return false;
    return select_image(new_image);
}

