#pragma once

#include <iostream>
#include <filesystem>
#include <fstream>

#include "../vendor/cxxopts.hpp"

#include "utils.hpp"

namespace fs = std::filesystem;

namespace orthorectify {

    class Parameters {

        const char* default_dem_path = "odm_dem/dsm.tif";
        const char* default_outdir = "orthorectified";
        const char* default_image_list = "img_list.txt";

		static std::vector<std::string> read_image_list(const std::string& image_list_path)
		{
			std::ifstream image_list_file(image_list_path);
			std::string line;
			std::vector<std::string> image_list;

			while (std::getline(image_list_file, line)) {
				trim_end(line);
				image_list.push_back(line);
			}

			return image_list;
		}

	public:

		fs::path dataset_path;
		std::string dem_path;
		InterpolationType interpolation;
		bool with_alpha;
		bool skip_visibility_test;

#ifdef _OPENMP
		int threads;
#endif

		bool verbose;
		fs::path outdir;
		std::vector<std::string> target_images;

		Parameters(int argc, const char* const* argv) {

			cxxopts::Options options("Orthorectify", "This tool is capable of orthorectifying individual images (or all images) from an existing ODM reconstruction.");

			options.add_options()
				("dataset", "Path to ODM dataset", cxxopts::value<std::string>())
				("e,dem", "Absolute path to DEM to use to orthorectify images", cxxopts::value<std::string>()->default_value(default_dem_path))
				("no-alpha", "Don't output an alpha channel", cxxopts::value<bool>()->default_value("false"))
				("i,interpolation", "Type of interpolation to use to sample pixel values (nearest, bilinear)", cxxopts::value<std::string>()->default_value("bilinear"))
				("o,outdir", "Output directory where to store results", cxxopts::value<std::string>()->default_value(default_outdir))
				("l,image-list", "Path to file that contains the list of image filenames to orthorectify. By default all images in a dataset are processed", cxxopts::value<std::string>()->default_value(default_image_list))
				("images", "Comma-separated list of filenames to rectify. Use as an alternative to --image-list", cxxopts::value<std::string>())
				("s,skip-visibility-test", "Skip visibility testing (faster but leaves artifacts due to relief displacement)", cxxopts::value<bool>()->default_value("false"))
#ifdef _OPENMP
				("t,threads", "Number of threads to use (-1 = all)", cxxopts::value<int>()->default_value("-1"))
#endif
				("v,verbose", "Verbose logging", cxxopts::value<bool>()->default_value("false"))
				("h,help", "Print usage")
				;

			options.allow_unrecognised_options();

			options.parse_positional({ "dataset" });

			const auto result = options.parse(argc, argv);

			if (result.count("help"))
			{
				std::cout << options.help();
				exit(0);
			}

			if (!result.count("dataset"))
			{
				std::cout << options.help();
				exit(1);
			}

			this->verbose = result["verbose"].count();
			this->dataset_path = result["dataset"].as<std::string>();

			const auto& dem = result["dem"].as<std::string>();
			this->dem_path = dem == default_dem_path ? (fs::path(dataset_path) / default_dem_path).generic_string() : dem;

			if (!fs::exists(this->dem_path))
			{
				std::cerr << "Error: DEM file '" << this->dem_path << "' does not exist" << std::endl;
				exit(1);
			}

			const auto tmpInterpolation = result["interpolation"].as<std::string>();

			if (tmpInterpolation == "bilinear")
				this->interpolation = Bilinear;
			else if (tmpInterpolation == "nearest")
				this->interpolation = Nearest;
			else
			{
				ERR << "Interpolation method " << this->interpolation << " is not supported";
				exit(1);
			}


			this->with_alpha = !result["no-alpha"].as<bool>();
			this->skip_visibility_test = result["skip-visibility-test"].as<bool>();

#ifdef _OPENMP
			this->threads = result["threads"].as<int>();

			if (this->threads < -1) {
				ERR << "Invalid number of threads: " << this->threads;
				exit(1);
			}
#endif

			const auto& outdir = result["outdir"].as<std::string>();

			if (result["images"].count()) {
				this->target_images = split(result["images"].as<std::string>(), ",");
			}
			else {
				const auto& tmp_image_list = result["image-list"].as<std::string>();
				const auto& image_list_path = tmp_image_list == default_image_list ? (fs::path(dataset_path) / default_image_list).generic_string() : tmp_image_list;

				if (!fs::exists(image_list_path))
				{
					std::cerr << "Error: Image list file '" << image_list_path << "' does not exist" << std::endl;
					exit(1);
				}

				this->target_images = read_image_list(image_list_path);
			}

			this->outdir = outdir == default_outdir ? fs::path(dataset_path) / default_outdir : fs::path(outdir);

		}

	};

}