#include "version.h"

#include "processing.hpp"
#include "parameters.hpp"
#include "dataset.hpp"

#include <plog/Log.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Init.h>

#include "customformatter.h"

using namespace orthorectify;

int main(int argc, char** argv)
{

	GDALAllRegister();

	const Parameters params(argc, argv);

	if (!fs::exists(params.outdir))
		fs::create_directories(params.outdir);

	plog::ColorConsoleAppender<plog::CleanTextFormatter> console_appender;
	plog::init(params.verbose ? plog::debug : plog::info, &console_appender);

	if (!params.target_images.empty()) {
		INF << "Processing " << params.target_images.size() << " images";

		// Print images
		for (const auto& image : params.target_images)
			DBG << image;
	}
	else
		INF << "Processing all images";

#ifdef _OPENMP
	if (params.threads == -1) {
		INF << "Using all available threads (" << omp_get_max_threads() << ")";
		omp_set_num_threads(omp_get_max_threads());
	}
	else {
		INF << "Using " << params.threads << " threads";
		omp_set_num_threads(params.threads);
	}
#endif

	INF << "Reading DEM: " << params.dem_path;

	const auto dem = static_cast<GDALDataset*>(GDALOpen(params.dem_path.c_str(), GA_ReadOnly));
	if (dem == nullptr)
	{
		ERR << "Could not open DEM file " << params.dem_path;
		exit(1);
	}

	// Get first raster band
	GDALRasterBand* dem_band = dem->GetRasterBand(1);
	if (dem_band == nullptr)
	{
		ERR << "Could not open DEM band";
		exit(1);
	}

	// Get DEM band data type
	const GDALDataType dem_band_type = dem_band->GetRasterDataType();
	if (dem_band_type != GDT_Float32 && dem_band_type != GDT_Byte && dem_band_type != GDT_UInt16)
	{
		ERR << "DEM band data type " << GDALGetDataTypeName(dem_band_type) << " is not supported";
		exit(1);
	} else {
		DBG << "DEM band type " << GDALGetDataTypeName(dem_band_type);
	}

	double dem_min_value, dem_max_value;
	get_band_min_max(dem_band, dem_min_value, dem_max_value);

	INF << "DEM Minimum: " << dem_min_value;
	INF << "DEM Maximum : " << dem_max_value;

	int dem_offset_x = 0;
	int dem_offset_y = 0;

	// Get CRS
	const char* tmp_wkt = dem->GetProjectionRef();

	std::string wkt;

	if (tmp_wkt != nullptr)
	{
		pretty_print_crs(tmp_wkt);
		get_dem_offsets(params.dataset_path, dem_offset_x, dem_offset_y);

		INF << "DEM offset (" << dem_offset_x << ", " << dem_offset_y << ")";

		wkt = tmp_wkt;
	}

	const int h = dem->GetRasterYSize();
	const int w = dem->GetRasterXSize();

	INF << "DEM dimensions: " << w << "x" << h << " pixels";

	int success;
	const double no_data = dem_band->GetNoDataValue(&success);

	const bool has_nodata = success != 0;

	if (has_nodata)
		DBG << "DEM NoData value: " << no_data;
	else {
		DBG << "DEM has no NoData value";
	}

	auto start = std::chrono::high_resolution_clock::now();

	INF << "Loading undistorted dataset";

	UndistortedDataset ds(params.dataset_path / "opensfm", params.dataset_path / "opensfm" / "undistorted");

	auto elapsed = std::chrono::high_resolution_clock::now() - start;

	INF << "Undistorted dataset loaded in " << human_duration(elapsed);

	DBG << "Found shots: ";
	// Print shots ids
	for (const auto& shot : ds.shots)
		DBG << shot.id;

	double geotransform[6];

	if (dem->GetGeoTransform(geotransform) != CE_None) {
		ERR << "Error getting geotransform";
		exit(1);
	}

	Transform transform(geotransform);

	void* dem_data;

	auto size = static_cast<size_t>(w) * h;

	switch (dem_band_type) {
	case GDT_Float32:
		dem_data = new float[size];
		break;
	case GDT_Byte:
		dem_data = new uint8_t[size];
		break;
	case GDT_UInt16:
		dem_data = new uint16_t[size];
		break;
	default:
		ERR << "Unexpected DEM band data type";
		exit(1);

	}

	if (dem_band->RasterIO(GF_Read, 0, 0, w, h, dem_data, w, h, dem_band_type, 0, 0) != CE_None) {
		ERR << "Error reading DEM";
		exit(1);
	}

	GDALClose(dem);

	DBG << "DEM data loaded";

	start = std::chrono::high_resolution_clock::now();

	auto cnt = 0;

#pragma omp parallel for
	for (auto s = 0; s < ds.shots.size(); s++)
	{
		const auto shot = ds.shots[s];

		if (std::find_if(params.target_images.begin(), params.target_images.end(),
			[&shot](const std::string& id) { return !id.compare(shot.id); }) == params.target_images.end())
		{
			DBG << "Skipping image " << shot.id;
			continue;
		}

		INF << "Processing shot " << shot.id;
		cnt++;

		const auto shot_ext = fs::path(shot.id).extension();

		// Add .tif if shot.id does not end with it
		const auto shot_file_name = shot_ext == ".tif" ? shot.id : shot.id + ".tif";

		const auto image_path = (params.dataset_path / "opensfm" / "undistorted" / "images" / shot_file_name).generic_string();

		DBG << "Image file path: " << image_path;
		const auto out_path = (params.outdir / shot_file_name).generic_string();

		switch (dem_band_type) {
		case GDT_Float32:
			process_image<float>(image_path, out_path, ProcessingParameters<float> {
				params.skip_visibility_test,
					shot,
					has_nodata,
					no_data,
					transform,
					static_cast<double>(dem_offset_x),
					static_cast<double>(dem_offset_y),
					w,
					h,
					dem_min_value,
					dem_max_value,
					static_cast<float*>(dem_data),
					params.interpolation,
					params.with_alpha,
					wkt
			}
			);
			break;
		case GDT_Byte:
			process_image<uint8_t>(image_path, out_path, ProcessingParameters<uint8_t> {
				params.skip_visibility_test,
					shot,
					has_nodata,
					no_data,
					transform,
					static_cast<double>(dem_offset_x),
					static_cast<double>(dem_offset_y),
					w,
					h,
					dem_min_value,
					dem_max_value,
					static_cast<uint8_t*>(dem_data),
					params.interpolation,
					params.with_alpha,
					wkt
			}
			);
			break;
		case GDT_UInt16:
			process_image<uint16_t>(image_path, out_path, ProcessingParameters<uint16_t> {
				params.skip_visibility_test,
					shot,
					has_nodata,
					no_data,
					transform,
					static_cast<double>(dem_offset_x),
					static_cast<double>(dem_offset_y),
					w,
					h,
					dem_min_value,
					dem_max_value,
					static_cast<uint16_t*>(dem_data),
					params.interpolation,
					params.with_alpha,
					tmp_wkt
			}
			);
			break;
		default:

			ERR << "Unexpected DEM band type";
			exit(1);
		}
	}

	switch (dem_band_type) {
	case GDT_Float32:
		delete[]static_cast<float*>(dem_data);
		break;
	case GDT_Byte:
		delete[]static_cast<uint8_t*>(dem_data);
		break;
	case GDT_UInt16:
		delete[]static_cast<uint16_t*>(dem_data);
		break;
	default:
		break;
	}

	elapsed = std::chrono::high_resolution_clock::now() - start;

	INF << "Processed " << cnt << " images in " << human_duration(elapsed);

	return 0;
}



