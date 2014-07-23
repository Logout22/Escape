#pragma once

#include <esc/common.h>
#include <string>
#include <gui/image/image.h>
#include <gui/imagebutton.h>

class ImageFile {
    private:
        std::shared_ptr<gui::ImageButton> image;
        std::string filename;
    public:
        ImageFile(const std::string fname) : filename(fname) {}
        void load_image() {
            image = std::shared_ptr<gui::ImageButton>(
                    new gui::ImageButton(gui::Image::loadImage(filename)));
        }
        // unload image to save memory
        void unload_image() { image.reset(); }
        std::shared_ptr<gui::ImageButton> get_image() { return image; }
};

