#pragma once

#include <iostream>
#include <filesystem>

#include "../vendor/json.hpp"

#include "utils.hpp"

#include "transform.hpp"
#include "dataset.hpp"
#include "rawimage.hpp"

namespace fs = std::filesystem;

namespace orthorectify {

	template <typename T>
	struct ProcessingParameters
	{

		const bool skip_visibility_test;
		const Shot& shot;
		const bool has_nodata;
		const float nodata_value;

		Transform& dem_transform;

		const double dem_offset_x;
		const double dem_offset_y;

		const int dem_width;
		const int dem_height;

		const double dem_min_value;
		const double dem_max_value;

		T* dem_data;

		const InterpolationType interpolation;
		const bool with_alpha;
		const std::string& wkt;

		const GDALDataType type;

	};

	template <typename T, typename S>
	void process_image(const std::string& in_path, const std::string& out_path, const ProcessingParameters<T>& params)
	{

		const auto start = std::chrono::high_resolution_clock::now();

		const auto& shot = params.shot;

		const auto Xs = shot.origin(0);
		const auto Ys = shot.origin(1);
		const auto Zs = shot.origin(2);

		const auto cam_x = Xs + params.dem_offset_x;
		const auto cam_y = Ys + params.dem_offset_y;

		double tmp_cam_grid_x, tmp_cam_grid_y;

		params.dem_transform.index(cam_x, cam_y, tmp_cam_grid_x, tmp_cam_grid_y);

		const int cam_grid_x = static_cast<int>(tmp_cam_grid_x);
		const int cam_grid_y = static_cast<int>(tmp_cam_grid_y);

		INF << "Rotation matrix: " << str_conv(shot.rotation_matrix);
		INF << "Origin: (" << shot.origin(0) << ", " << shot.origin(1) << ", " << shot.origin(2) << ")";
		INF << "DEM index: (" << cam_grid_x << ", " << cam_grid_y << ")";
		INF << "Camera pose: (" << Xs << ", " << Ys << ", " << Zs << ")";

		std::vector<float> distance_map;
		float* distance_map_raw = nullptr;

		const auto h = params.dem_height;
		const auto w = params.dem_width;

		if (!params.skip_visibility_test)
		{
			distance_map.resize(h * w);
			distance_map_raw = distance_map.data();

			for (auto j = 0; j < h; j++) {
				for (auto i = 0; i < w; i++) {
					const auto val = sqrt((cam_grid_x - i) * (cam_grid_x - i) + (cam_grid_y - j) * (cam_grid_y - j));
					distance_map_raw[j * w + i] = (float)(val == 0 ? 1e-7 : val);
				}
			}

			DBG << "Populated distance map";
		}

		RawImage<S> image(in_path);

		const int img_w = image.width();
		const int img_h = image.height();
		const int half_img_w = static_cast<int>((static_cast<double>(img_w) - 1) / 2.0);
		const int half_img_h = static_cast<int>((static_cast<double>(img_h) - 1) / 2.0);

		const int bands = image.bands();

		const auto f = shot.camera_focal * MAX(img_h, img_w);

		DBG << "Camera focal: " << shot.camera_focal << " coefficient " << f;
		INF << "Image dimensions: " << img_w << "x" << img_h << " pixels (" << bands << " bands with type " << GDALGetDataTypeName(image.type()) << ")";

		const auto a1 = shot.rotation_matrix(0, 0);
		const auto b1 = shot.rotation_matrix(0, 1);
		const auto c1 = shot.rotation_matrix(0, 2);
		const auto a2 = shot.rotation_matrix(1, 0);
		const auto b2 = shot.rotation_matrix(1, 1);
		const auto c2 = shot.rotation_matrix(1, 2);
		const auto a3 = shot.rotation_matrix(2, 0);
		const auto b3 = shot.rotation_matrix(2, 1);
		const auto c3 = shot.rotation_matrix(2, 2);

		DemInfo info {
			a1, b1, c1,
			a2, b2, c2,
			a3, b3, c3,
			Xs, Ys, Zs,
			f,
			params.dem_min_value,
			params.dem_offset_x,
			params.dem_offset_y,
			params.dem_transform
		};

		double dem_ul_x, dem_ul_y;
		info.get_coordinates(-half_img_w, -half_img_h, dem_ul_x, dem_ul_y);

		double dem_ur_x, dem_ur_y;
		info.get_coordinates(half_img_w, -half_img_h, dem_ur_x, dem_ur_y);

		double dem_lr_x, dem_lr_y;
		info.get_coordinates(half_img_w, half_img_h, dem_lr_x, dem_lr_y);

		double dem_ll_x, dem_ll_y;
		info.get_coordinates(-half_img_w, half_img_h, dem_ll_x, dem_ll_y);

		const auto x_list = { dem_ul_x, dem_ur_x, dem_lr_x, dem_ll_x };
		const auto y_list = { dem_ul_y, dem_ur_y, dem_lr_y, dem_ll_y };

		DBG << "DEM bounding box: (" << dem_ul_x << ", " << dem_ul_y << "), (" << dem_ur_x << ", " <<
			dem_ur_y << "), (" << dem_lr_x << ", " << dem_lr_y << "), (" << dem_ll_x << ", " <<
			dem_ll_y << ")";

		const int dem_bbox_minx = MIN(w - 1, MAX(0, static_cast<int>(std::min(x_list))));
		const int dem_bbox_miny = MIN(h - 1, MAX(0, static_cast<int>(std::min(y_list))));
		const int dem_bbox_maxx = MIN(w - 1, MAX(0, static_cast<int>(std::max(x_list))));
		const int dem_bbox_maxy = MIN(h - 1, MAX(0, static_cast<int>(std::max(y_list))));

		const int dem_bbox_w = 1 + dem_bbox_maxx - dem_bbox_minx;
		const int dem_bbox_h = 1 + dem_bbox_maxy - dem_bbox_miny;

		INF << "Iterating over DEM box: [(" << dem_bbox_minx << ", " << dem_bbox_miny << "), (" << dem_bbox_maxx << ", " << dem_bbox_maxy << ")] (" << dem_bbox_w << "x" << dem_bbox_h << " pixels)";

		RawImage<S> imgout(dem_bbox_w, dem_bbox_h, bands, "GTiff", params.type);

		std::vector<S> values(bands);
		memset(values.data(), 0, values.size() * sizeof(S));

		auto minx = dem_bbox_w;
		auto miny = dem_bbox_h;
		auto maxx = 0;
		auto maxy = 0;

		std::vector<Point> points;
		const auto cnt = static_cast<int>(std::sqrt(cam_grid_x * cam_grid_x + cam_grid_y * cam_grid_y));
		points.resize(cnt);
		auto* raw_points = points.data();
		auto* raw_dem_data = params.dem_data;

		for (auto j = dem_bbox_miny; j < dem_bbox_maxy + 1; j++) {

			auto im_j = j - dem_bbox_miny;

			for (auto i = dem_bbox_minx; i < dem_bbox_maxx + 1; i++) {

				auto im_i = i - dem_bbox_minx;

				const auto Za = raw_dem_data[j * w + i];

				// Skip nodata
				if (params.has_nodata && Za == params.nodata_value)
					continue;

				double Xa, Ya;
				params.dem_transform.xy_center(i, j, Xa, Ya);

				// Remove offset(our cameras don't have the geographic offset)
				Xa -= params.dem_offset_x;
				Ya -= params.dem_offset_y;

				// Colinearity function http ://web.pdx.edu/~jduh/courses/geog493f14/Week03.pdf
				const auto dx = (Xa - Xs);
				const auto dy = (Ya - Ys);
				const auto dz = (Za - Zs);

				const auto den = a3 * dx + b3 * dy + c3 * dz;
				const auto x = half_img_w - (f * (a1 * dx + b1 * dy + c1 * dz) / den);
				const auto y = half_img_h - (f * (a2 * dx + b2 * dy + c2 * dz) / den);

				if (x >= 0 && y >= 0 && x <= img_w - 1 && y <= img_h - 1)
				{
					//DBG << "Working on pixel (" << i << ", " << j << ") -> (" << im_i << ", " << im_j << ")" ;
					//DBG << "DEM coordinates: (" << Xa << ", " << Ya << ", " << Za << ")" << " -> (" << Xa << ", " << Ya << ")" ;

					if (!params.skip_visibility_test)
					{
						auto cnt = 0;
						line(i, j, cam_grid_x, cam_grid_y, raw_points, cnt);

						bool visible = true;
						for (auto p = 0; p < cnt; p++)
						{
							const auto point = raw_points + p;
							const auto px = point->x;
							const auto py = point->y;

							if (px < 0 || py < 0 || px >= w || py >= h)
								continue;

							const auto ray_z = Zs + dz * (distance_map_raw[py * w + px] / distance_map_raw[j * w + i]);

							if (ray_z > params.dem_max_value) break;

							if (raw_dem_data[py * w + px] > ray_z) {
								visible = false;

								//DBG << "Point (" << p.x << ", " << p.y << ") is not visible" ;

								break;
							}
						}

						if (!visible)
							continue;
					}

					if (params.interpolation == Bilinear)
					{
						const auto xi = img_w - 1 - x;
						const auto yi = img_h - 1 - y;

						image.bilinear_interpolate(xi, yi, values.data());
					}
					else
					{
						const auto xi = img_w - 1 - static_cast<int>(std::round(x));
						const auto yi = img_h - 1 - static_cast<int>(std::round(y));

						image.get_pixel(xi, yi, values.data());
					}

					// We don't consider all zero values (pure black)
					// to be valid sample values. This will sometimes miss
					// valid sample values.
					if (std::any_of(values.begin(), values.end(), [](const auto& v) { return v != 0; }))
					{
						minx = MIN(minx, im_i);
						miny = MIN(miny, im_j);
						maxx = MAX(maxx, im_i);
						maxy = MAX(maxy, im_j);

						imgout.set_pixel(im_i, im_j, values.data());
						//DBG << "Boundaries updated: (" << minx << ", " << miny << ") -> (" << maxx << ", " << maxy << ")" ;
					}

					//DBG << "Setting pixel (" << im_i << ", " << im_j << ") to (" << static_cast<int>(values[0]) << ", " << static_cast<int>(values[1]) << ", " << static_cast<int>(values[2]) << ")" ;

				}
			}
		}


		/*#ifdef DEBUG
				DBG << "Writing intermediate output image" ;
				imgout.write(out_path + ".intermediate.tif", "");
		#endif*/

		INF << "Output bounds (" << minx << ", " << miny << "), (" << maxx << ", " << maxy << ") pixels";

		if (minx > maxx || miny > maxy)
		{
			ERR << "Cannot orthorectify image (is the image inside the DEM bounds?)";
			return;
		}

		const auto out_w = maxx - minx + 1;
		const auto out_h = maxy - miny + 1;

		const auto target_bands = params.with_alpha ? bands + 1 : bands;

		RawImage<S> imgdst(out_w, out_h, target_bands, "GTiff", params.type);

		values.resize(target_bands);
		memset(values.data(), 0, target_bands * sizeof(S));

		if (params.with_alpha) {

			// Copy the data
			for (auto j = 0; j < out_h; ++j)
			{
				for (auto i = 0; i < out_w; ++i)
				{
					const auto im_i = minx + i;
					const auto im_j = miny + j;

					imgout.get_pixel(im_i, im_j, values.data());

					bool nan = true;

					for (auto b = 0; b < bands; ++b)
					{
						if (values[b] != 0)
						{
							nan = false;
							break;
						}
					}
					values[target_bands - 1] = nan ? 0 : 255;

					imgdst.set_pixel(i, j, values.data());

				}
			}

		}
		else {

			// Copy the data
			for (auto j = 0; j < out_h; ++j)
			{
				for (auto i = 0; i < out_w; ++i)
				{
					const auto im_i = minx + i;
					const auto im_j = miny + j;

					imgout.get_pixel(im_i, im_j, values.data());
					imgdst.set_pixel(i, j, values.data());
				}
			}
		}


		double offset_x, offset_y;
		params.dem_transform.xy(dem_bbox_minx + minx, dem_bbox_miny + miny, offset_x, offset_y);

		imgdst.write(out_path, "", [&params, offset_x, offset_y, out_w, out_h, bands](GDALDataset* ds) {

			double transform[6] = {
				params.dem_transform[0],
				params.dem_transform[1],
				offset_x,
				params.dem_transform[3],
				params.dem_transform[4],
				offset_y
			};

			ds->SetGeoTransform(transform);

			// Set width and height, is this necessary?
			ds->SetMetadataItem("WIDTH", std::to_string(out_w).c_str());
			ds->SetMetadataItem("HEIGHT", std::to_string(out_h).c_str());

			ds->SetMetadataItem("SOFTWARE", "Orthorectify");

			// Set projection (if any)
			if (!params.wkt.empty())
				ds->SetProjection(params.wkt.c_str());

			});

		const auto elapsed = std::chrono::high_resolution_clock::now() - start;

		INF << "Orthorectified image \"" << shot.id << "\" written in " << human_duration(elapsed);

	}
}