#include "directory.h"
#include <iostream>

using namespace std;

bool Directory::change_path(const std::string &path) {
    try {
        file new_dir(path);
        if (!new_dir.is_dir()) return false;
        // by default do not list hidden files:
        vector<sDirEntry> entries = new_dir.list_files(false, "*.bmp");
        for (auto const &it : entries) {
            image_files.push_back(shared_ptr<ImageFile>(
                        new ImageFile(path, it.name)));
        }
        current_path = path;
        select_image(0);
    } catch(const default_error& e) {
        cerr << e.what() << endl;
        return false;
	}
    return true;
}

bool Directory::select_image(size_t new_index) {
    image_files[current_index]->unload_image();
    image_files[new_index]->load_image();
    // update status bar
    statbar->set_filename(image_files[new_index]->get_filename());
    current_index = new_index;
    return true;
}

