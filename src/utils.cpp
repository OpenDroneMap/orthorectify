#include <iostream>
#include <filesystem>
#include <fstream>

#include "utils.hpp"


namespace orthorectify {

	std::vector<std::string> split(const std::string& s, const std::string& delimiter) {

		size_t pos_start = 0, pos_end;
		const size_t delim_len = delimiter.length();
		std::vector<std::string> res;

		while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos) {
			std::string token = s.substr(pos_start, pos_end - pos_start);
			pos_start = pos_end + delim_len;
			res.push_back(token);
		}

		res.push_back(s.substr(pos_start));
		return res;
	}

    void trim_end(std::string& str) {
		str.erase(std::find_if(str.rbegin(), str.rend(), [](const int ch) {
			return !std::isspace(ch);
			}).base(), str.end());
	}

    std::string human_duration(std::chrono::nanoseconds elapsed) {
		std::stringstream ss;

		const auto hours = std::chrono::duration_cast<std::chrono::hours>(elapsed);
		elapsed -= hours;
		const auto minutes = std::chrono::duration_cast<std::chrono::minutes>(elapsed);
		elapsed -= minutes;
		const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed);
		elapsed -= seconds;
		const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);

		if (hours.count() > 0)
			ss << hours.count() << "h ";
		if (minutes.count() > 0)
			ss << minutes.count() << "m ";
		if (seconds.count() > 0)
			ss << seconds.count() << "s ";
		if (milliseconds.count() > 0)
			ss << milliseconds.count() << "ms";

		return ss.str();
	}

    void get_dem_offsets(const fs::path& dataset_path, int& dem_offset_x, int& dem_offset_y)
	{
		// Reads coords.txt
		const auto coords_path = dataset_path / "odm_georeferencing" / "coords.txt";

		if (!fs::exists(coords_path))
		{
			ERR << "Error: could not find coords.txt file at " << coords_path;
			exit(1);
		}

		std::ifstream coords_file(coords_path.string());

		if (!coords_file.is_open())
		{
			ERR << "Error: could not open coords.txt file at " << coords_path;
			exit(1);
		}

		std::string line;
		std::getline(coords_file, line); // Skip first line
		std::getline(coords_file, line); // Read second line

		// Trim trailing spaces
		trim_end(line);

		const auto segs = split(line, " ");

		dem_offset_x = std::stoi(segs[0]);
		dem_offset_y = std::stoi(segs[1]);

		coords_file.close();
	}

    void pretty_print_crs(const char*& demWkt)
	{
		OGRSpatialReference demSrs;
		demSrs.importFromWkt(&demWkt);
		char* ptr;

		demSrs.exportToProj4(&ptr);
		INF << "DEM CRS (proj): " << ptr;

		demSrs.exportToPrettyWkt(&ptr);
		DBG << "DEM CRS (wkt): " << std::endl << std::endl << ptr << std::endl;

	}

	void get_band_min_max(GDALRasterBand* demBand, double& dem_min_value, double& dem_max_value, bool approximate)
	{
		double adfMinMax[2];

		// I don't know if we have to use no approximation and scan all the pixels (sounds very inefficient to me)
		if (demBand->ComputeRasterMinMax(approximate ? 1 : 0, adfMinMax) != CE_None || adfMinMax[0] == adfMinMax[1])
		{
			ERR << "Error: could not compute DEM min/max";
			exit(1);
		}

		dem_min_value = adfMinMax[0];
		dem_max_value = adfMinMax[1];

	}

	void print_bands_info(GDALDataset* ds)
	{
		const auto bands = ds->GetRasterCount();

		for (auto band = 0; band < bands; band++)
		{
			auto* poBand = ds->GetRasterBand(band + 1);

			int nBlockXSize, nBlockYSize;

			poBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
			DBG << "Band " << band << ": block=" << nBlockXSize << "x" << nBlockYSize << " Type=" << GDALGetDataTypeName(poBand->GetRasterDataType()) <<
				", ColorInterp=" << GDALGetColorInterpretationName(poBand->GetColorInterpretation());

		}
	}

    // Generate line pixel coordinates
	void line(int startx, int starty, int endx, int endy, Point* out, int& cnt) {

		const auto dx = endx - startx;
		const auto dy = endy - starty;

		const auto abs_dx = ABS(dx);
		const auto abs_dy = ABS(dy);

		const auto sx = dx > 0 ? 1 : -1;
		const auto sy = dy > 0 ? 1 : -1;

		auto err = abs_dx - abs_dy;

		auto idx = 0;

		while (true) {

			auto& p = out[idx];

			p.x = startx;
			p.y = starty;

			if (startx == endx && starty == endy)
				break;

			const auto e2 = 2 * err;

			if (e2 > -abs_dy) {
				err -= abs_dy;
				startx += sx;
			}

			if (e2 < abs_dx) {
				err += abs_dx;
				starty += sy;
			}

			idx++;

		}

		cnt = idx + 1;

	}

	std::string str_conv(const Mat3d& mtrx)
	{
		std::ostringstream stream;
		for (auto i = 0; i < mtrx.rows(); i++) {
			for (auto j = 0; j < mtrx.cols(); j++) {
				stream << mtrx(i, j) << " ";
			}
			stream;
		}
		return stream.str();
	}


}