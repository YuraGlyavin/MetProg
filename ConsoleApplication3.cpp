#define _USE_MATH_DEFINES
#include <cmath>
#include <limits>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include "geometry.h"

class Lite {
public:
	Lite(const Vec3rd1& p, const float i) : position(p), intensity(i) {}
	Vec3rd1 position;
	float intensity;
};
class Mat{
public:
	Mat(const float r, const Vec4th & a, const Vec3rd1 & color, const float spec) : refractive_index(r), vect(a), diffuse_color(color), specular_exponent(spec) {}
	Mat() : refractive_index(1), vect(1,0,0,0), diffuse_color(), specular_exponent() {}
private:
	  friend class Rend;
	float refractive_index;
 Vec4th vect;
 Vec3rd1 diffuse_color;
	float specular_exponent;
};

class Sfera {
private:
	Vec3rd1 center;
	float radius;
	Mat material;
	friend class Rend;
	bool ray_intersect(const Vec3rd1& orig, const Vec3rd1& dir, float& t0) const {
		Vec3rd1 L = center - orig;
		float tca = L * dir;
		float d2 = L * L - tca * tca;
		if (d2 > radius* radius) return false;
		float thc = sqrtf(radius * radius - d2);
		t0 = tca - thc;
		float t1 = tca + thc;
		if (t0 < 0) t0 = t1;
		if (t0 < 0) return false;
		return true;
	}
public:
	Sfera(const Vec3rd1& c, const float r, const Mat& m) : center(c), radius(r), material(m) {}

};

class Rend {
public:
	Rend() {}
	void render(const std::vector<Sfera>& spheres, const std::vector<Lite>& lights) {
		const int   width = 1024;
		const int   height = 768;
		const float fov = M_PI / 3.;
		std::vector<Vec3rd1> framebuffer(width * height);

		for (size_t j = 0; j < height; j++) { // actual rendering loop
			for (size_t i = 0; i < width; i++) {
				float dir_x = (i + 0.5) - width / 2.;
				float dir_y = -(j + 0.5) + height / 2.;    // this flips the image at the same time
				float dir_z = -height / (2. * tan(fov / 2.));
				framebuffer[i + j * width] = cast_ray(Vec3rd1(0, 0, 0), Vec3rd1(dir_x, dir_y, dir_z).normalize(), spheres, lights);
			}
		}

		std::ofstream ofs; // save the framebuffer to file
		ofs.open("./out.ppm", std::ios::binary);
		ofs << "P6\n" << width << " " << height << "\n255\n";
		for (size_t i = 0; i < height * width; ++i) {
			Vec3rd1& c = framebuffer[i];
			float max = std::max(c[0], std::max(c[1], c[2]));
			if (max > 1) c = c * (1. / max);
			for (size_t j = 0; j < 3; j++) {
				ofs << (char)(255 * std::max(0.f, std::min(1.f, framebuffer[i][j])));
			}
		}
		ofs.close();
	}
private:
	Vec3rd1 refract(const Vec3rd1& I, const Vec3rd1& N, const float eta_t, const float eta_i = 1.f) { // Snell's law
		float cosi = -std::max(-1.f, std::min(1.f, I * N));
		if (cosi < 0) return refract(I, -N, eta_i, eta_t); // if the ray comes from the inside the object, swap the air and the media
		float eta = eta_i / eta_t;
		float k = 1 - eta * eta * (1 - cosi * cosi);
		return k < 0 ? Vec3rd1(1, 0, 0) : I * eta + N * (eta * cosi - sqrtf(k)); // k<0 = total reflection, no ray to refract. I refract it anyways, this has no physical meaning
	}
	bool scene_intersect(const Vec3rd1& orig, const Vec3rd1& dir, const std::vector<Sfera>& spheres, Vec3rd1& hit, Vec3rd1& N, Mat& material) {
		float spheres_dist = std::numeric_limits<float>::max();
		for (size_t i = 0; i < spheres.size(); i++) {
			float dist_i;
			if (spheres[i].ray_intersect(orig, dir, dist_i) && dist_i < spheres_dist) {
				spheres_dist = dist_i;
				hit = orig + dir * dist_i;
				N = (hit - spheres[i].center).normalize();
				material = spheres[i].material;
			}
		}

		float checkerboard_dist = std::numeric_limits<float>::max();
		if (fabs(dir.y) > 1e-3) {
			float d = -(orig.y + 4) / dir.y; // the checkerboard plane has equation y = -4
			Vec3rd1 pt = orig + dir * d;
			if (d > 0 && fabs(pt.x) < 10 && pt.z<-10 && pt.z>-30 && d < spheres_dist) {
				checkerboard_dist = d;
				hit = pt;
				N = Vec3rd1(0, 1, 0);
				material.diffuse_color = (int(.5 * hit.x + 1000) + int(.5 * hit.z)) & 1 ? Vec3rd1(.3, .3, .3) : Vec3rd1(.3, .2, .1);
			}
		}
		return std::min(spheres_dist, checkerboard_dist) < 1000;
	}

	Vec3rd1 cast_ray(const Vec3rd1& orig, const Vec3rd1& dir, const std::vector<Sfera>& spheres, const std::vector<Lite>& lights, size_t depth = 0) {
		Vec3rd1 point, N;
		Mat material;

		if (!scene_intersect(orig, dir, spheres, point, N, material)) {
			return Vec3rd1(0.2, 0.7, 0.8); // background color
		}


		Vec3rd1 refract_dir = refract(dir, N, material.refractive_index).normalize();
		Vec3rd1 refract_orig = refract_dir * N < 0 ? point - N * 1e-3 : point + N * 1e-3;
		Vec3rd1 refract_color = cast_ray(refract_orig, refract_dir, spheres, lights, depth + 1);

		float diffuse_light_intensity = 0, specular_light_intensity = 0;
		for (size_t i = 0; i < lights.size(); i++) {
			Vec3rd1 light_dir = (lights[i].position - point).normalize();

			diffuse_light_intensity += lights[i].intensity * std::max(0.f, light_dir * N);
			specular_light_intensity += powf(0.f, material.specular_exponent) * lights[i].intensity;
		}
		return material.diffuse_color * diffuse_light_intensity * material.vect[0] + Vec3rd1(1., 1., 1.) * specular_light_intensity * material.vect[1] + refract_color * material.vect[3];
	}

};




int main() {
	Mat      ivory(1.0, Vec4th(0.6, 0.3, 0.1, 0.0), Vec3rd1(0.4, 0.4, 0.3), 50.);
	Mat red_rubber(1.0, Vec4th(0.9, 0.1, 0.0, 0.0), Vec3rd1(0.3, 0.1, 0.1), 10.);

	std::vector<Sfera> spheres;
	spheres.push_back(Sfera(Vec3rd1(-3, 0, -16), 2, ivory));
	spheres.push_back(Sfera(Vec3rd1(1.5, -0.5, -18), 3, red_rubber));
	std::vector<Lite>  lights;
	lights.push_back(Lite(Vec3rd1(30, 50, -25), 3));
	Rend renderer;
	renderer.render(spheres, lights);

	return 0;
}