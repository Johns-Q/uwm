///
///	@file image.h	@brief image loading header file
///
///	Copyright (c) 2009 by Lutz Sammer.  All Rights Reserved.
///
///	Contributor(s):
///
///	License: AGPLv3
///
///	This program is free software: you can redistribute it and/or modify
///	it under the terms of the GNU Affero General Public License as
///	published by the Free Software Foundation, either version 3 of the
///	License.
///
///	This program is distributed in the hope that it will be useful,
///	but WITHOUT ANY WARRANTY; without even the implied warranty of
///	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
///	GNU Affero General Public License for more details.
///
///	$Id$
//////////////////////////////////////////////////////////////////////////////

/// @addtogroup image
/// @{

#ifdef USE_ICON

//////////////////////////////////////////////////////////////////////////////
//	Declares
//////////////////////////////////////////////////////////////////////////////

/**
**	Image typedef.
*/
typedef struct _image_ Image;

/**
**	Image structure.
*/
struct _image_
{
    uint16_t Width;			///< width of image
    uint16_t Height;			///< height of image
    uint8_t Data[1];			///< image data ARGB 32
};

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

#ifdef USE_JPEG
    /// Load a JPEG image from given file name.
extern Image *ImageLoadJPEG0(const char *name, unsigned, unsigned);
#endif

#ifdef USE_XPM
    /// Create an image from XPM data.
extern Image *ImageFromData(const char *const *);
#endif

    /// Create an image from EWMH data.
extern Image *ImageFromEWMH(const uint32_t *);

    /// Load an image from the specified file.
extern Image *ImageLoadFile(const char *);

    /// Destroy an image.
extern void ImageDel(Image *);

#else // }{ USE_ICON

#endif // } !USE_ICON

/// @}
