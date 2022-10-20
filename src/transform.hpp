#pragma once

#include <iostream>

namespace orthorectify {

    class Transform {

        double _geotransform[6];

        public:

        Transform(double geotransform[6]) {
            memcpy(this->_geotransform, geotransform, sizeof(double) * 6);
        }

        // Override [] operator
        double operator[](int index) {
            return this->_geotransform[index];
        }

        inline void index(const double x, const double y, double& out_x, double& out_y)
        {
            out_x = (x - _geotransform[0]) / _geotransform[1];
            out_y = (y - _geotransform[3]) / _geotransform[5];
        }

        inline void xy_center(const double x, const double y, double& out_x, double& out_y)
        {
            out_x = (x + 0.5) * _geotransform[1] + _geotransform[0];
            out_y = (y + 0.5) * _geotransform[5] + _geotransform[3];
        }

        inline void xy(const double x, const double y, double& out_x, double& out_y)
        {
            out_x = x * _geotransform[1] + _geotransform[0];
            out_y = y * _geotransform[5] + _geotransform[3];
        }

    };

    struct DemInfo
	{
		double a1;
		double b1;
		double c1;
		double a2;
		double b2;
		double c2;
		double a3;
		double b3;
		double c3;
		double Xs;
		double Ys;
		double Zs;
		double f;
		double dem_min_value;
		double dem_offset_x;
		double dem_offset_y;
		Transform& transform;
	};

	inline void dem_coordinates(
		const int cpx,
		const int cpy,
		const DemInfo& info,
		double& x,
		double& y)
	{

		const auto a1 = info.a1;
		const auto b1 = info.b1;
		const auto c1 = info.c1;
		const auto a2 = info.a2;
		const auto b2 = info.b2;
		const auto c2 = info.c2;
		const auto a3 = info.a3;
		const auto b3 = info.b3;
		const auto c3 = info.c3;

		const auto Xs = info.Xs;
		const auto Ys = info.Ys;
		const auto Zs = info.Zs;

		const auto f = info.f;

		const auto Za = info.dem_min_value;
		const auto m = (a3 * b1 * cpy - a1 * b3 * cpy - (a3 * b2 - a2 * b3) * cpx - (a2 * b1 - a1 * b2) * f);
		const auto Xa = static_cast<double>(info.dem_offset_x) + (m * Xs + (b3 * c1 * cpy - b1 * c3 * cpy - (b3 * c2 - b2 * c3) * cpx - (b2 * c1 - b1 * c2) * f) * Za - (b3 * c1 * cpy - b1 * c3 * cpy - (b3 * c2 - b2 * c3) * cpx - (b2 * c1 - b1 * c2) * f) * Zs) / m;
		const auto Ya = static_cast<double>(info.dem_offset_y) + (m * Ys - (a3 * c1 * cpy - a1 * c3 * cpy - (a3 * c2 - a2 * c3) * cpx - (a2 * c1 - a1 * c2) * f) * Za + (a3 * c1 * cpy - a1 * c3 * cpy - (a3 * c2 - a2 * c3) * cpx - (a2 * c1 - a1 * c2) * f) * Zs) / m;

        info.transform.index(Xa, Ya, x, y);

		//transform_coordinates(info.geotransform, Xa, Ya, x, y);

	}


}