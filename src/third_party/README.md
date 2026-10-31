# Image codecs

`stb_image.h` and `stb_image_write.h` are vendored from nothings/stb,
commit `003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5`.
The dual public-domain/MIT license is embedded in each header. Only PNG
reading is enabled. PNG writing is used for the opt-in native tile catalog.
WebP is decoded by Android BitmapFactory or optional Linux libwebp; neither
libwebp nor Android platform source is bundled here.
