#pragma once

#include <esc/common.h>
#include <string>
#include <gui/imagebutton.h>
#include <file.h>

class ImageFile {
    private:
        std::shared_ptr<gui::ImageButton> image;
        std::unique_ptr<std::file> image_file_handle;
    public:
        ImageFile(const std::string &path, const std::string &fname);
        void load_image();
        // unload image to save memory
        void unload_image() { image.reset(); }
        std::string get_filename() { return image_file_handle->name(); }
        std::shared_ptr<gui::ImageButton> get_image() { return image; }
};

