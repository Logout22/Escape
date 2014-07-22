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
    public:
        // XXX
        Directory(std::shared_ptr<StatusBar> stb, const std::string &path)
            : statbar(stb)
        { change_path(path); }
        bool change_path(const std::string &path);
};

