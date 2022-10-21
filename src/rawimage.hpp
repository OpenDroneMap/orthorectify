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

		static void _throw_last_error()
		{
			auto* err = CPLGetLastErrorMsg();
			ERR << err;
			throw std::runtime_error(err);
		}

		static void _get_min_max(GDALRasterBand* band, double& min, double& max)
		{
			int bGotMin, bGotMax;
			double adfMinMax[2];

			adfMinMax[0] = band->GetMinimum(&bGotMin);
			adfMinMax[1] = band->GetMaximum(&bGotMax);

			if (!(bGotMin && bGotMax))
				GDALComputeRasterMinMax(band, TRUE, adfMinMax);

			min = adfMinMax[0];
			max = adfMinMax[1];
			//printf("Min=%.3fd, Max=%.3f\n", adfMinMax[0], adfMinMax[1]);
		}

		void _load(const std::string& path)
		{

			if (!fs::exists(path)) {
				ERR << "File " << path << " does not exist";
				throw std::runtime_error("File does not exist");
			}

			auto* ds = static_cast<GDALDataset*>(GDALOpen(path.c_str(), GA_ReadOnly));

			if (ds == nullptr) {
				auto* err = CPLGetLastErrorMsg();
				ERR << "Could not open image at " << path;
				_throw_last_error();
			}

			this->_driver = ds->GetDriverName();
			this->_width = ds->GetRasterXSize();
			this->_height = ds->GetRasterYSize();
			this->_has_alpha = false;
			this->_bands = 3;

			const size_t size = this->_width * this->_height;

			const auto bands = ds->GetRasterCount();
			const auto type = ds->GetRasterBand(1)->GetRasterDataType();

			if (bands == 3 || bands == 4) {

				if (type == GDT_Byte) {

					this->R = new uint8_t[size];
					this->G = new uint8_t[size];
					this->B = new uint8_t[size];

					ds->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, this->_width, this->_height, this->R, this->_width, this->_height, GDT_Byte, 0, 0);
					ds->GetRasterBand(2)->RasterIO(GF_Read, 0, 0, this->_width, this->_height, this->G, this->_width, this->_height, GDT_Byte, 0, 0);
					ds->GetRasterBand(3)->RasterIO(GF_Read, 0, 0, this->_width, this->_height, this->B, this->_width, this->_height, GDT_Byte, 0, 0);

					if (bands == 4)
					{
						this->A = new uint8_t[size];
						ds->GetRasterBand(4)->RasterIO(GF_Read, 0, 0, this->_width, this->_height, this->A, this->_width, this->_height, GDT_Byte, 0, 0);
						this->_has_alpha = true;
						this->_bands = 4;
					}
				}
				else {
					ERR << "Unsupported image type" << GDALGetDataTypeName(type);
					GDALClose(ds);
					throw std::runtime_error("Unsupported image type");
				}

			}
			else if (bands == 2) {
				ERR << "Unsupported image with 2 bands and type " << GDALGetDataTypeName(type);
				GDALClose(ds);
				throw std::runtime_error("Unsupported image with 2 bands");
			}
			else if (bands == 1)
			{

				if (type == GDT_Byte) {

					this->R = new uint8_t[size];
					this->G = new uint8_t[size];
					this->B = new uint8_t[size];

					ds->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, this->_width, this->_height, this->R, this->_width, this->_height, GDT_Byte, 0, 0);

					memcpy(this->G, this->R, size);
					memcpy(this->B, this->R, size);

				}
				else if (type == GDT_UInt16) {

					this->R = new uint8_t[size];
					this->G = new uint8_t[size];
					this->B = new uint8_t[size];

					auto* r = new uint16_t[this->_width * this->_height];

					const auto b = ds->GetRasterBand(1);
					double min, max;
					_get_min_max(b, min, max);

					b->RasterIO(GF_Read, 0, 0, this->_width, this->_height, r, this->_width, this->_height, GDT_UInt16, 0, 0);

					for (auto i = 0; i < this->_width * this->_height; i++) {

						const auto scaled = static_cast<uint8_t>((r[i] - min) / (max - min) * 256);

						this->R[i] = scaled;
						this->G[i] = scaled;
						this->B[i] = scaled;
					}

					delete[] r;

				}
				else if (type == GDT_UInt32) {

					this->R = new uint8_t[size];
					this->G = new uint8_t[size];
					this->B = new uint8_t[size];

					auto* r = new uint32_t[this->_width * this->_height];

					const auto b = ds->GetRasterBand(1);
					double min, max;
					_get_min_max(b, min, max);
					
					b->RasterIO(GF_Read, 0, 0, this->_width, this->_height, r, this->_width, this->_height, GDT_UInt32, 0, 0);
					
					for (auto i = 0; i < this->_width * this->_height; i++) {

						const auto scaled = static_cast<uint8_t>((r[i] - min) / (max - min) * 256);

						this->R[i] = scaled;
						this->G[i] = scaled;
						this->B[i] = scaled;
					}

					delete[] r;

				}
				else
				{
					ERR << "Unsupported image type " << GDALGetDataTypeName(type);
					GDALClose(ds);
					throw std::runtime_error("Unsupported image type");
				}
			}
			else {
				ERR << "Unsupported image type" << GDALGetDataTypeName(type);
				GDALClose(ds);
				throw std::runtime_error("Unsupported image type");

			}

			GDALClose(ds);
		}

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
		

		void get_pixel(const int x, const int y, uint8_t* out) const
		{

#if DEBUG
			if (x >= _width || y >= _height || x < 0 || y < 0) {
				ERR << "Invalid pixel access: " << x << ", " << y;
				throw std::runtime_error("Invalid pixel access");
			}
#endif
			const auto idx = IDX(x, y);

			out[0] = R[idx];
			out[1] = G[idx];
			out[2] = B[idx];

			if (_has_alpha)
				out[3] = A[idx];

		}

		void set_pixel(const int x, const int y, const uint8_t* in)
		{

#if DEBUG
			if (x >= _width || y >= _height || x < 0 || y < 0) {
				ERR << "Invalid pixel access: " << x << ", " << y;
				throw std::runtime_error("Invalid pixel access");
			}
#endif

			const auto idx = IDX(x, y);

			R[idx] = in[0];
			G[idx] = in[1];
			B[idx] = in[2];

			if (_has_alpha)
				A[idx] = in[3];

		}		

		void bilinear_interpolate(const double x, const double y, uint8_t* out) const
		{
			auto x0 = static_cast<int>(std::floor(x));
			auto x1 = x0 + 1;
			auto y0 = static_cast<int>(std::floor(y));
			auto y1 = y0 + 1;

			const auto width = _width;
			const auto height = _height;

			x0 = std::clamp(x0, 0, width - 1);
			x1 = std::clamp(x1, 0, width - 1);

			y0 = std::clamp(y0, 0, height - 1);
			y1 = std::clamp(y1, 0, height - 1);

			const auto wa = (x1 - x) * (y1 - y);
			const auto wb = (x1 - x) * (y - y0);
			const auto wc = (x - x0) * (y1 - y);
			const auto wd = (x - x0) * (y - y0);

			out[0] = wa * R[y0 * width + x0] + wb * R[y1 * width + x0] + wc * R[y0 * width + x1] + wd * R[y1 * width + x1];
			out[1] = wa * G[y0 * width + x0] + wb * G[y1 * width + x0] + wc * G[y0 * width + x1] + wd * G[y1 * width + x1];
			out[2] = wa * B[y0 * width + x0] + wb * B[y1 * width + x0] + wc * B[y0 * width + x1] + wd * B[y1 * width + x1];

			if (_has_alpha)
			{
				out[3] = wa * A[y0 * width + x0] + wb * A[y1 * width + x0] + wc * A[y0 * width + x1] + wd * A[y1 * width + x1];
			}

		}

		void write(const std::string& path, const std::string& driver, const std::function<void(GDALDataset*)>& configure) const
		{

			if (R == nullptr || G == nullptr || B == nullptr) {
				ERR << "No data to write";
				return;
			}

			if (fs::exists(path))
				fs::remove(path);

			auto* mem_driver = GetGDALDriverManager()->GetDriverByName("MEM");
			auto* dst_driver = GetGDALDriverManager()->GetDriverByName(driver.empty() ? _driver.c_str() : driver.c_str());

			auto* mem_ds = mem_driver->Create("", _width, _height, _has_alpha ? 4 : 3, GDT_Byte, nullptr);

			if (mem_ds == nullptr) {
				ERR << "Could not create in-memory dataset";
				_throw_last_error();
			}

			if (configure != nullptr) configure(mem_ds);

			if (mem_ds->GetRasterBand(1)->RasterIO(GF_Write, 0, 0, _width, _height, this->R, _width, _height, GDT_Byte, 0, 0) != CE_None) {
				ERR << "Could not write red raster band";
				GDALClose(mem_ds);
				_throw_last_error();
			}

			if (mem_ds->GetRasterBand(2)->RasterIO(GF_Write, 0, 0, _width, _height, this->G, _width, _height, GDT_Byte, 0, 0) != CE_None) {
				ERR << "Could not write green raster band";
				GDALClose(mem_ds);
				_throw_last_error();
			}

			if (mem_ds->GetRasterBand(3)->RasterIO(GF_Write, 0, 0, _width, _height, this->B, _width, _height, GDT_Byte, 0, 0) != CE_None) {
				ERR << "Could not write blue raster band";
				GDALClose(mem_ds);
				_throw_last_error();
			}

			if (_has_alpha)
			{
				if (mem_ds->GetRasterBand(4)->RasterIO(GF_Write, 0, 0, _width, _height, this->A, _width, _height, GDT_Byte, 0, 0) != CE_None) {
					ERR << "Could not write alpha raster band";
					GDALClose(mem_ds);
					_throw_last_error();
				}
			}
			
			auto* ds = dst_driver->CreateCopy(path.c_str(), mem_ds, 0, nullptr, nullptr, nullptr);

			if (ds == nullptr) {

				// Last error
				ERR << "Could not create image at " << path;
				GDALClose(mem_ds);
				_throw_last_error();
			}

			GDALClose(ds);
			GDALClose(mem_ds);

		}
	};

}