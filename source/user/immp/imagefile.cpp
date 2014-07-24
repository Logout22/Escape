#include "imagefile.h"
#include <gui/image/image.h>

using namespace std;
using namespace gui;
using namespace esc;

ImageFile::ImageFile(const string &path, const string &fname) {
    image_file_handle = unique_ptr<file>(new file(path, fname));
}

void ImageFile::load_image() {
    image = sptr<ImageButton>(Image::loadImage(image_file_handle->path()));
}

