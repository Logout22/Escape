#pragma once

#include <sys/common.h>
#include <string>
#include <gui/imagebutton.h>
#include <esc/file.h>

class ImageFile {
    private:
        std::shared_ptr<gui::ImageButton> image;
        std::unique_ptr<esc::file> image_file_handle;
    public:
        ImageFile(const std::string &path, const std::string &fname);
        void load_image();
        // unload image to save memory
        void unload_image() { image.reset(); }
        std::string get_filename() { return image_file_handle->name(); }
        std::shared_ptr<gui::ImageButton> get_image() { return image; }
};

