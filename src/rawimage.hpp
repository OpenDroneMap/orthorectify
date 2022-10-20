#pragma once

#include <iostream>
#include <filesystem>

#include "../vendor/json.hpp"

#include "utils.hpp"

#include "gdal_priv.h"

namespace fs = std::filesystem;

namespace orthorectify {

    class RawImage {

		int _width;
		int _height;
		int _bands;

		std::vector<uint8_t> _data;
		std::string _path;

		std::string _driver;

		static void _throw_last_error()
		{
			auto* err = CPLGetLastErrorMsg();
			ERR << err;
			throw std::runtime_error(err);
		}

		void _load();
	public:
		explicit RawImage(const std::string& path) {

			this->_path = path;
			this->_width = 0;
			this->_height = 0;
			this->_bands = 0;

			_load();
		}

		RawImage(int width, int height, int bands, const std::string& driver) {

			this->_width = width;
			this->_height = height;
			this->_bands = bands;
			this->_driver = driver;

			this->_data.resize(width * height * bands);
			memset(this->_data.data(), 0, this->_data.size());
		}

		void write(const std::string& path, const std::string& driver = "", const std::function<void(GDALDataset*)> configure = nullptr);

		int width() const { return _width; }
		int height() const { return _height; }
		int bands() const { return _bands; }
		std::string driver() const { return _driver; }

		uint8_t get_pixel(const int x, const int y, const int band) const;

		// This will be a performance bottleneck
		void get_pixel(const int x, const int y, uint8_t* out) const;

		// This will be a performance bottleneck too
		void set_pixel(const int x, const int y, const uint8_t* in);

		void set_pixel(const int x, const int y, const int band, const uint8_t in);

		void bilinear_interpolate(const double x, const double y, uint8_t* out) const;


	};

}