/**
 * $Id$
 * Copyright (C) 2008 - 2014 Nils Asmussen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#pragma once

#include <sys/common.h>
#include <gui/graphics/graphics.h>
#include <exception>
#include <memory>

namespace gui {
	class img_load_error : public std::exception {
	public:
		img_load_error(const std::string& str) throw ()
			: exception(), _str(str) {
		}
		~img_load_error() throw () {
		}
		virtual const char *what() const throw () {
			return _str.c_str();
		}
	private:
		std::string _str;
	};

	class Image {
	public:
		static std::shared_ptr<Image> loadImage(const std::string& path);

	public:
		Image() {
		}
		virtual ~Image() {
		}

		virtual Size getSize() const = 0;

		virtual void paint(Graphics &g,const Pos &pos) = 0;
	};
}
