#include "imagefile.h"
#include <gui/image/image.h>

using namespace std;

ImageFile::ImageFile(const string &path, const string &fname) {
    image_file_handle = unique_ptr<file>(new file(path, fname));
}

void ImageFile::load_image() {
    image = std::shared_ptr<gui::ImageButton>(new gui::ImageButton(
                gui::Image::loadImage(image_file_handle->path())));
}

