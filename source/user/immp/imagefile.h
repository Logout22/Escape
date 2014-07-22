#pragma once

#include <esc/common.h>
#include <string>
#include <gui/imagebutton.h>

class ImageFile {
    private:
        std::shared_ptr<ImageButton> image;
        std::string filename;
    public:
        ImageFile(const std::string fname);
};

