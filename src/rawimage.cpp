
#include "rawimage.hpp"

namespace orthorectify {

    void RawImage::_load()
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

        _width = ds->GetRasterXSize();
        _height = ds->GetRasterYSize();

        this->_bands = ds->GetRasterCount();

        _data.resize(_height * _width * _bands);

        if (ds->RasterIO(GF_Read, 0, 0, _width, _height, _data.data(), _width, _height, GDT_Byte, _bands, nullptr, 0, 0, 0) != CE_None) {

            auto* err = CPLGetLastErrorMsg();
            ERR << "Error reading image";
            ERR << err;

            GDALClose(ds);
            throw std::runtime_error(err);
        }

        GDALClose(ds);
    }

    void RawImage::write(const std::string& path, const std::string& driver, const std::function<void(GDALDataset*)> configure) {

        if (_data.empty()) {
            ERR << "No data to write";
            return;
        }

        if (fs::exists(path))
            fs::remove(path);

        auto* mem_driver = GetGDALDriverManager()->GetDriverByName("MEM");
        auto* dst_driver = GetGDALDriverManager()->GetDriverByName(driver.empty() ? _driver.c_str() : driver.c_str());

        auto* mem_ds = mem_driver->Create("", _width, _height, _bands, GDT_Byte, nullptr);

        if (mem_ds == nullptr) {
            ERR << "Could not create in-memory dataset";
            _throw_last_error();
        }

        if (configure != nullptr) configure(mem_ds);

        if (mem_ds->RasterIO(GF_Write, 0, 0, _width, _height, _data.data(), _width, _height, GDT_Byte, _bands, nullptr, 0, 0, 0) != CE_None) {
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

    uint8_t RawImage::get_pixel(const int x, const int y, const int band) const
    {

#if DEBUG
        if (x >= _width || y >= _height || band >= _bands || x < 0 || y < 0 || band < 0) {
            ERR << "Invalid pixel access: " << x << ", " << y << ", " << band;
            throw std::runtime_error("Invalid pixel access");
        }
#endif

        return _data[y * _width + x + _height * _width * band];
    }

    // This will be a performance bottleneck
    void RawImage::get_pixel(const int x, const int y, uint8_t* out) const
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

    // This will be a performance bottleneck too
    void RawImage::set_pixel(const int x, const int y, const uint8_t* in)
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

    void RawImage::set_pixel(const int x, const int y, const int band, const uint8_t in) {

#if DEBUG
        if (x >= _width || y >= _height || band >= _bands || x < 0 || y < 0 || band < 0) {
            ERR << "Invalid pixel access: " << x << ", " << y << ", " << band;
            throw std::runtime_error("Invalid pixel access");
        }
#endif

        _data[y * _width + x + _height * _width * band] = in;
    }

    void RawImage::bilinear_interpolate(const double x, const double y, uint8_t* out) const
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

			out[band] = static_cast<uint8_t>(wa * Ia + wb * Ib + wc * Ic + wd * Id);

		}

	}


}