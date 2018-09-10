#include "physics.hpp"

#include <omp.h>
#include <Eigen/Geometry>

physics::physics()
{
	this->generator = std::mt19937_64(std::random_device{}());

}

physics::~physics()
{

}

void physics::step(double delta_t)
{
	total_time += delta_t;

	// TODO: collision detection
	#pragma omp parallel for
	for(int i = 0; i < obj_count; i++)
	{
		// Euler
		//a[current][i] = accel(x[current][i], i);
		//v[next][i] = a[current][i] * delta_t + v[current][i];
		//x[next][i] = v[next][i] * delta_t + x[current][i];

		// RK4 integration, see doc/grav_sim.tex
		// this is about 4x worse than Euler
		Eigen::Vector3d xk1 = x[current][i];
		Eigen::Vector3d vk1 = v[current][i];
		Eigen::Vector3d ak1 = accel(x[current][i], i);

		Eigen::Vector3d xk2 = x[current][i] + 0.5 * vk1 * delta_t;
		Eigen::Vector3d vk2 = v[current][i] + 0.5 * ak1 * delta_t;
		Eigen::Vector3d ak2 = accel(xk1, i);

		Eigen::Vector3d xk3 = x[current][i] + 0.5 * vk2 * delta_t;
		Eigen::Vector3d vk3 = v[current][i] + 0.5 * ak2 * delta_t;
		Eigen::Vector3d ak3 = accel(xk3, i);

		Eigen::Vector3d xk4 = x[current][i] + vk3 * delta_t;
		Eigen::Vector3d vk4 = v[current][i] + ak3 * delta_t;
		Eigen::Vector3d ak4 = accel(xk4, i);

		v[next][i] = v[current][i] + (delta_t / 6.0) *
				(ak1 + 2 * ak2 + 2 * ak3 + ak4);
		x[next][i] = x[current][i] + (delta_t / 6.0) *
				(vk1 + 2 * vk2 + 2 * vk3 + vk4);
	}

	current = current ? 0 : 1;
	next = next ? 0 : 1;
}

Eigen::Vector3d physics::accel(Eigen::Vector3d x_i, uint16_t skip_index)
{
	Eigen::Vector3d a(0.0, 0.0, 0.0);
	for(uint16_t j = 0; j < obj_count; j++)
	{
		if(skip_index == j)
			continue;

		Eigen::Vector3d r = x[current][j] - x_i;
		a += G * m[j] * r.normalized() / r.squaredNorm();
	}
	return a;
}

void physics::init(uint16_t obj_count)
{
	this->obj_count = obj_count;

	current = 0;
	next = 1;
	total_time = 0.0;

	x[0].resize(obj_count);
	x[1].resize(obj_count);
	v[0].resize(obj_count);
	v[1].resize(obj_count);
	a[0].resize(obj_count);
	a[1].resize(obj_count);
	r.resize(obj_count);
	m.resize(obj_count);

	// for now hard code some values
	mass_range[0] = 5e7;
	mass_range[1] = 1e8;
	radius_range[0] = 0.05;
	radius_range[1] = 0.25;
	//distance_range[0] = -radius_range[0] * 15.0;
	//distance_range[1] = radius_range[0] * 15.0;
	distance_range[0] = -4.0;
	distance_range[1] = 4.0;

	// random init stuff
	std::uniform_real_distribution<double> dist_m(mass_range[0],
		mass_range[1]);
	std::uniform_real_distribution<double> dist_r(radius_range[0],
		radius_range[1]);
	std::uniform_real_distribution<double> dist_d(distance_range[0],
		distance_range[1]);

	for(uint16_t i = 0; i < obj_count; i++)
	{
		r[i] = dist_r(generator);
		m[i] = dist_m(generator);

		// TODO: detect and avoid collisions here
		bool collision = true;
		while(collision)
		{
			// generate a new point
			x[0][i] = Eigen::Vector3d(dist_d(generator),
									dist_d(generator),
									dist_d(generator));

			// look for a collision
			collision = false;
			for(uint16_t j = 0; j < i; j++)
			{
				if(i == j)
					continue;

				// direction unit vector
				Eigen::Vector3d ji_uv = (x[0][i] - x[0][j]).normalized();

				// position plus radius in the direction of the other object
				Eigen::Vector3d ji_r = x[0][j] + ji_uv * r[j];

				double distance_j = (x[0][i] - ji_r).norm();
				// if radius > distance to j object then collision
				if(r[i] > distance_j)
				{
					collision = true;
					break;
				}
			}
		}

		v[0][i] = Eigen::Vector3d(0.0, 0.0, 0.0);
		a[0][i] = Eigen::Vector3d(0.0, 0.0, 0.0);
	}
}

void physics::deinit()
{
	// TODO: should we really use swap to force a deallocation and is this the
	// right way to do it?

	std::vector<Eigen::Vector3d> n0, n1;
	x[0].clear();
	x[1].clear();
	x[0].swap(n0);
	x[1].swap(n1);

	std::vector<Eigen::Vector3d> n2, n3;
	v[0].clear();
	v[1].clear();
	v[0].swap(n2);
	v[1].swap(n3);

	std::vector<Eigen::Vector3d> n4, n5;
	a[0].clear();
	a[1].clear();
	a[0].swap(n4);
	a[1].swap(n5);

	std::vector<double> n6;
	r.clear();
	r.swap(n6);

	std::vector<double> n7;
	m.clear();
	m.swap(n7);
}


