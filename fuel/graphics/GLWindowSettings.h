/*****************************************************************
 * GLWindowSettings.h
 *****************************************************************
 * Created on: 11.06.2015
 * Author: HAUSWALD, Tom.
 *****************************************************************
 *****************************************************************/

#ifndef GRAPHICS_GLWINDOWSETTINGS_H_
#define GRAPHICS_GLWINDOWSETTINGS_H_

#include <cstdint>
#include <string>

namespace fuel
{
	/**
	 * OpenGL window settings.
	 */
	struct GLWindowSettings
	{
		//Window width in pixels
		uint16_t width;

		//Window height in pixels
		uint16_t height;

		//Whether fullscreen mode is active
		bool fullscreen;

		//Window title
		std::string title;
	};
}

#endif // GRAPHICS_GLWINDOWSETTINGS_H_
