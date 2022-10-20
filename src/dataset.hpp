
#pragma once

#include <iostream>
#include <filesystem>
#include <fstream>

#include "../vendor/json.hpp"
#include "utils.hpp"

#include "types.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace orthorectify {

	struct CameraModel {

		std::string id;
		int width;
		int height;
		double focal;

		std::string projection_type;

		CameraModel(const std::string& id, json& camera_model) {
			this->id = id;

			projection_type = camera_model.contains("projection_type") ? camera_model["projection_type"] : "perspective";

			if (projection_type == "perspective") {
				this->focal = camera_model["focal"];
			}
			else if (projection_type == "brown") {
				this->focal = camera_model["focal_x"];
			}
			else if (projection_type == "fisheye") {
				this->focal = camera_model["focal"];
			}
			else if (projection_type == "fisheye_opencv") {
				this->focal = camera_model["focal"];
			}
			else if (projection_type == "fisheye62") {
				this->focal = camera_model["focal_x"];
			}
			else if (projection_type == "fisheye624") {
				this->focal = camera_model["focal_x"];
			}
			else if (projection_type == "radial") {
				this->focal = camera_model["focal_x"];
			}
			else if (projection_type == "simple_radial") {
				this->focal = camera_model["focal_x"];
			}
			else if (projection_type == "dual") {
				this->focal = camera_model["focal"];
			}
			else if (projection_type == "spherical") {
				this->focal = 0;
			}
			else {
				ERR << "Unrecognised projection type: " << projection_type;
				exit(1);
			}

			this->width = camera_model["width"];
			this->height = camera_model["height"];

		}

	};

    struct Shot {

		std::string id;
		Mat3d rotation_matrix;
		Vec3d origin;
		double camera_focal;

		Shot(const std::string& id, json& shot, std::vector<CameraModel>& camera_models) {

			this->id = id;

			//DBG << "Reading shot " << id ;

			std::string camera_id = shot["camera"];

			// Find camera model in camera_models
			const auto camera = std::find_if(camera_models.begin(), camera_models.end(),
				[&camera_id](const CameraModel& cm) { return cm.id == camera_id; });

			if (camera == camera_models.end()) {
				ERR << "Error: could not find camera model \"" << camera_id << "\" for shot " << id;
				exit(1);
			}

			this->camera_focal = camera->focal;

			const auto& rotation = shot["rotation"];
			const auto& translation = shot["translation"];

			const double rx = rotation[0];
			const double ry = rotation[1];
			const double rz = rotation[2];

			const double tx = translation[0];
			const double ty = translation[1];
			const double tz = translation[2];

			const Vec3d vr(rx, ry, rz);
			const Mat3d mr = VectorToRotationMatrix(vr);

			const Vec3d vt(tx, ty, tz);

			Mat4d world_to_cam;

			world_to_cam.setIdentity();
			world_to_cam.block<3, 3>(0, 0) = mr;
			world_to_cam.block<3, 1>(0, 3) = vt;

			this->rotation_matrix = world_to_cam.block<3, 3>(0, 0);
			this->origin = world_to_cam.inverse().block<3, 1>(0, 3);

		}

	private:

		static Mat3d VectorToRotationMatrix(const Vec3d& r) {
			const auto n = r.norm();
			return (n == 0 ?
				Eigen::AngleAxisd(0, r) :
				Eigen::AngleAxisd(n, r / n)).toRotationMatrix();
		}

	};

	class UndistortedDataset {

		fs::path folder;
		fs::path path;

		static std::vector<CameraModel> get_camera_models(json cameras)
		{

			std::vector<CameraModel> camera_models_vector;

			for (auto& [key, val] : cameras.items())
				camera_models_vector.emplace_back(key, val);

			return camera_models_vector;

		}

	public:

		std::vector<Shot> shots;

		UndistortedDataset(const fs::path& folder, const fs::path& path) {
			this->folder = folder;
			this->path = path;

			const auto filename = folder / "reconstruction.json";

			DBG << "Loading reconstruction from " << filename;

			// Load json
			std::ifstream json_file(filename.string());
			if (!json_file.is_open()) {
				ERR << "Could not open reconstruction file at " << filename;
				exit(1);
			}

			json reconstructions;
			json_file >> reconstructions;
			json_file.close();

			if (reconstructions.empty()) {
				ERR << "No reconstructions found in " << filename;
				exit(1);
			}

			auto& reconstruction = reconstructions[0];

			// Load reconstruction shots
			if (!reconstruction.count("shots")) {
				ERR << "No shots found in " << filename;
				exit(1);
			}

			auto& shots = reconstruction["shots"];
			auto& cameras = reconstruction["cameras"];

			auto camera_models = get_camera_models(cameras);

			for (auto& [key, val] : shots.items()) {
				this->shots.emplace_back(key, val, camera_models);
			}
		}
	};
}