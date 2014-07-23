#pragma once

#include <esc/common.h>
#include <string>
#include <file.h>
#include <list>
#include "imagefile.h"
#include "statusbar.h"

typedef std::vector< std::shared_ptr<ImageFile> > ImageFileVector;
typedef ImageFileVector::iterator ImageFileIterator;

class Directory {
    private:
        std::unique_ptr<std::file> dirhandle;
        ImageFileVector image_files;
        ImageFileIterator current_image;
        std::shared_ptr<StatusBar> statbar;

        bool select_image(ImageFileIterator new_image);
    public:
        Directory(std::shared_ptr<StatusBar> stb, const std::string &path)
            : statbar(stb)
        { change_path(path); }
        bool change_path(const std::string &path);
        bool select_prev() { return select_image(current_image - 1); }
        bool select_next() { return select_image(current_image + 1); }
        std::shared_ptr<ImageFile> get_current_image();
};

