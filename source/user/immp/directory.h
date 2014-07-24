#pragma once

#include <sys/common.h>
#include <string>
#include <esc/file.h>
#include <list>
#include "imagefile.h"
#include "statusbar.h"

typedef std::vector< std::shared_ptr<ImageFile> > ImageFileVector;
typedef ImageFileVector::iterator ImageFileIterator;

class Directory {
    private:
        std::unique_ptr<esc::file> dirhandle;
        ImageFileVector image_files;
        ImageFileIterator current_image;
        std::shared_ptr<StatusBar> statbar;

        bool select_image(ImageFileIterator new_image);
        void load_image(std::shared_ptr<ImageFile> file_to_load);
        void update_statusbar(const std::string &fname);
    public:
        Directory(std::shared_ptr<StatusBar> stb, const std::string &path)
            : statbar(stb)
        { change_path(path); }
        bool change_path(const std::string &path);
        bool select_prev();
        bool select_next();
        std::shared_ptr<ImageFile> get_current_image();
};

