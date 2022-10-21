#pragma once

#include <iostream>
#include <filesystem>

#include "../vendor/json.hpp"

#include "utils.hpp"

#include "gdal_priv.h"

#define IDX(x, y) (x + y * _width)

namespace fs = std::filesystem;

namespace orthorectify {

	class RawImage
	{
		int _width;
		int _height;
		bool _has_alpha;
		int _bands;
		std::string _driver;

		uint8_t* R;
		uint8_t* G;
		uint8_t* B;
		uint8_t* A;

		void _throw_last_error();
		void _get_min_max(GDALRasterBand* band, double& min, double& max);
		void _load(const std::string& path);

	public:

		int width() const { return _width; }
		int height() const { return _height; }
		bool has_alpha() const { return _has_alpha; }
		int bands() const { return _bands; }

		RawImage(const std::string& path) {

			this->R = nullptr;
			this->G = nullptr;
			this->B = nullptr;
			this->A = nullptr;

			this->_width = 0;
			this->_height = 0;
			this->_has_alpha = false;
			this->_bands = 0;

			_load(path);
		}

		RawImage(int width, int height, bool has_alpha, const std::string& driver) {

			this->_width = width;
			this->_height = height;
			this->_has_alpha = has_alpha;
			this->_bands = has_alpha ? 4 : 3;
			this->_driver = driver;

			const size_t size = this->_width * this->_height;

			this->R = new uint8_t[size];
			memset(this->R, 0, size);
			this->G = new uint8_t[size];
			memset(this->G, 0, size);
			this->B = new uint8_t[size];
			memset(this->B, 0, size);

			if (has_alpha)
			{
				this->A = new uint8_t[size];
				memset(this->A, 0, size);
			}
			else
				this->A = nullptr;

		}

		~RawImage()
		{
			delete[] R;
			delete[] G;
			delete[] B;
			delete[] A;
		}


		void get_pixel(const int x, const int y, uint8_t* out) const;
		void set_pixel(const int x, const int y, const uint8_t* in);
		void bilinear_interpolate(const double x, const double y, uint8_t* out) const;
		void write(const std::string& path, const std::string& driver, const std::function<void(GDALDataset*)>& configure);
	};

}