
#pragma once

#include <iostream>
#include <filesystem>
#include <fstream>

#include <plog/Log.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Appenders/ConsoleAppender.h>

#include "gdal_priv.h"
#include "types.hpp"

#define INF PLOGI
#define DBG PLOGD
#define ACT PLOGI
#define ERR PLOGE

namespace orthorectify {

	namespace fs = std::filesystem;

	enum InterpolationType
	{
		Nearest = 1,
		Bilinear = 2
	};

	std::vector<std::string> split(const std::string& s, const std::string& delimiter);
    void trim_end(std::string& str);
	std::string human_duration(std::chrono::nanoseconds elapsed);
	void get_dem_offsets(const fs::path& dataset_path, int& dem_offset_x, int& dem_offset_y);

	void pretty_print_crs(const char*& demWkt);
	void get_band_min_max(GDALRasterBand* demBand, double& dem_min_value, double& dem_max_value, bool approximate = false);
	void print_bands_info(GDALDataset* ds);

	struct Point {
		int x;
		int y;
	};

	// Generate line pixel coordinates
	void line(int startx, int starty, int endx, int endy, Point* out, int& cnt);

	std::string str_conv(const Mat3d& mtrx);



}