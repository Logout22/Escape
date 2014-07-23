#include "directory.h"
#include <iostream>

using namespace std;

bool Directory::change_path(const string &path) {
    try {
        auto new_dir = unique_ptr<file>(new file(path));
        if (!new_dir->is_dir()) return false;
        image_files.clear();
        // by default do not list hidden files:
        // XXX handle all file endings
        for (auto const &it : new_dir->list_files(false, "*.bmp")) {
            image_files.push_back(shared_ptr<ImageFile>(
                        new ImageFile(path, it.name)));
        }
        dirhandle = unique_ptr<file>(new file(*new_dir));
        current_image = image_files.end();
        select_image(image_files.begin());
    } catch(const default_error& e) {
        cerr << e.what() << endl;
        return false;
	}
    return true;
}

bool Directory::select_image(ImageFileIterator new_image) {
    if (new_image == image_files.end()) return false;
    if (current_image != image_files.end()) {
        (*current_image)->unload_image();
    }
    (*new_image)->load_image();
    // update status bar
    statbar->set_filename((*new_image)->get_filename());
    current_image = new_image;
    return true;
}

shared_ptr<ImageFile> Directory::get_current_image() {
    return current_image ? (*current_image) : shared_ptr<ImageFile>();
}

