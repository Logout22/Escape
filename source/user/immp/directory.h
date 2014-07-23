#pragma once

#include <esc/common.h>
#include <string>
#include <file.h>
#include <list>
#include "imagefile.h"
#include "statusbar.h"

class Directory {
    private:
        std::unique_ptr<std::file> dirhandle;
        std::vector< std::shared_ptr<ImageFile> > image_files;
        std::shared_ptr<StatusBar> statbar;
        std::string current_path;
        size_t current_index;
    public:
        // XXX
        Directory(std::shared_ptr<StatusBar> stb, const std::string &path)
            : statbar(stb)
        { change_path(path); }
        bool change_path(const std::string &path);
        bool select_image(size_t new_index);
        bool select_prev() { return select_image(current_index - 1); }
        bool select_next() { return select_image(current_index + 1); }
        std::shared_ptr<ImageFile> get_current_image()
            { return image_files[current_index]; }
};

