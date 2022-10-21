#pragma once

#include <iostream>
#include <filesystem>

#include "../vendor/json.hpp"

#include "utils.hpp"

#include "gdal_priv.h"

namespace fs = std::filesystem;

namespace orthorectify {

	template<typename T>
    class RawImage {

		int _width;
		int _height;
		int _bands;
		GDALDataType _type;

		std::vector<T> _data;
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
			_load();
		}

		RawImage(int width, int height, int bands, const std::string& driver, GDALDataType type) {

			this->_width = width;
			this->_height = height;
			this->_bands = bands;
			this->_driver = driver;
			this->_type = type;

			this->_data.resize(width * height * bands * (int)type);
			memset(this->_data.data(), 0, this->_data.size() * (int)type);
		}

		void write(const std::string& path, const std::string& driver = "", const std::function<void(GDALDataset*)> configure = nullptr);

		int width() const { return _width; }
		int height() const { return _height; }
		int bands() const { return _bands; }
		GDALDataType type() const { return _type; }
		std::string driver() const { return _driver; }

		T get_pixel(const int x, const int y, const int band) const;

		// This will be a performance bottleneck
		void get_pixel(const int x, const int y, T* out) const;

		// This will be a performance bottleneck too
		void set_pixel(const int x, const int y, const T* in);

		void set_pixel(const int x, const int y, const int band, const T in);

		void bilinear_interpolate(const double x, const double y, T* out) const;

	};

    template<typename T>
    void RawImage<T>::_load()
    {

        if (!fs::exists(_path)) {
            ERR << "File " << _path << " does not exist";
            throw std::runtime_error("File does not exist");
        }

        auto* ds = static_cast<GDALDataset*>(GDALOpen(_path.c_str(), GA_ReadOnly));

        if (ds == nullptr) {

            auto* err = CPLGetLastErrorMsg();
            ERR << "Could not open image at " << _path;
            ERR << err;
            throw std::runtime_error(err);
        }

        this->_driver = ds->GetDriverName();
        this->_width = ds->GetRasterXSize();
        this->_height = ds->GetRasterYSize();
        this->_bands = ds->GetRasterCount();
        this->_type = ds->GetRasterBand(1)->GetRasterDataType();

        _data.resize(_height * _width * _bands * (int)_type);

        if (ds->RasterIO(GF_Read, 0, 0, _width, _height, _data.data(), _width, _height, _type, _bands, nullptr, 0, 0, 0) != CE_None) {

            auto* err = CPLGetLastErrorMsg();
            ERR << "Error reading image";
            ERR << err;

            GDALClose(ds);
            throw std::runtime_error(err);
        }

        GDALClose(ds);
    }

    template<typename T>
    void RawImage<T>::write(const std::string& path, const std::string& driver, const std::function<void(GDALDataset*)> configure) {

        if (_data.empty()) {
            ERR << "No data to write";
            return;
        }

        if (fs::exists(path))
            fs::remove(path);

        auto* mem_driver = GetGDALDriverManager()->GetDriverByName("MEM");
        auto* dst_driver = GetGDALDriverManager()->GetDriverByName(driver.empty() ? _driver.c_str() : driver.c_str());

        auto* mem_ds = mem_driver->Create("", _width, _height, _bands, _type, nullptr);

        if (mem_ds == nullptr) {
            ERR << "Could not create in-memory dataset";
            _throw_last_error();
        }

        if (configure != nullptr) configure(mem_ds);

        if (mem_ds->RasterIO(GF_Write, 0, 0, _width, _height, _data.data(), _width, _height, _type, _bands, nullptr, 0, 0, 0) != CE_None) {
            ERR << "Could not write to " << path;
            GDALClose(mem_ds);
            _throw_last_error();
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

    template<typename T>
    T RawImage<T>::get_pixel(const int x, const int y, const int band) const
    {

#if DEBUG
        if (x >= _width || y >= _height || band >= _bands || x < 0 || y < 0 || band < 0) {
            ERR << "Invalid pixel access: " << x << ", " << y << ", " << band;
            throw std::runtime_error("Invalid pixel access");
        }
#endif

        return _data[y * _width + x + _height * _width * band];
    }

    template<typename T>
    void RawImage<T>::get_pixel(const int x, const int y, T* out) const
    {

#if DEBUG
        if (x >= _width || y >= _height || x < 0 || y < 0) {
            ERR << "Invalid pixel access: " << x << ", " << y;
            throw std::runtime_error("Invalid pixel access");
        }
#endif

        for (auto band = 0; band < _bands; band++)
            out[band] = _data[y * _width + x + _height * _width * band];

    }

    template<typename T>
    void RawImage<T>::set_pixel(const int x, const int y, const T* in)
    {

#if DEBUG
        if (x >= _width || y >= _height || x < 0 || y < 0) {
            ERR << "Invalid pixel access: " << x << ", " << y;
            throw std::runtime_error("Invalid pixel access");
        }
#endif

        for (auto band = 0; band < _bands; band++)
            _data[y * _width + x + _height * _width * band] = in[band];
    }

    template<typename T>
    void RawImage<T>::set_pixel(const int x, const int y, const int band, const T in) {

#if DEBUG
        if (x >= _width || y >= _height || band >= _bands || x < 0 || y < 0 || band < 0) {
            ERR << "Invalid pixel access: " << x << ", " << y << ", " << band;
            throw std::runtime_error("Invalid pixel access");
        }
#endif

        _data[y * _width + x + _height * _width * band] = in;
    }

    template<typename T>
    void RawImage<T>::bilinear_interpolate(const double x, const double y, T* out) const
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

        const int bands = _bands;

        for (auto band = 0; band < bands; band++) {

            const auto Ia = this->get_pixel(x0, y0, band);
            const auto Ib = this->get_pixel(x0, y1, band);
            const auto Ic = this->get_pixel(x1, y0, band);
            const auto Id = this->get_pixel(x1, y1, band);

            out[band] = static_cast<T>(wa * Ia + wb * Ib + wc * Ic + wd * Id);

        }

    }

}