#include "directory.h"
#include <iostream>

using namespace std;

bool Directory::change_path(const std::string &path) {
    try {
        file new_dir(path);
        if (!new_dir.is_dir()) return false;
        // by default do not list hidden files:
        vector<sDirEntry> entries = new_dir.list_files(false);
        for (auto it : entries) {
            //XXX continue
        }
    } catch(const default_error& e) {
        cerr << e.what() << endl;
        return false;
	}
    return true;
}

bool Directory::select_image(size_t new_index) {
    // update status bar
    return true;
}

