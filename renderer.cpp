#define _CRT_SECURE_NO_WARNINGS 1
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <cmath>
#include<iostream>

using namespace std;

static inline double sqr(double x) { return x * x; }

class Vector {
public:
	explicit Vector(double x = 0, double y = 0, double z = 0) {
		coord[0] = x;
		coord[1] = y;
		coord[2] = z;
	}
	double& operator[](int i) { return coord[i]; }
	double operator[](int i) const { return coord[i]; }

	Vector& operator+=(const Vector& v) {
		coord[0] += v[0];
		coord[1] += v[1];
		coord[2] += v[2];
		return *this;
	}

	double norm2() const {
		return sqr(coord[0]) + sqr(coord[1]) + sqr(coord[2]);
	}

    double normalize() {
        double n = sqrt(this->norm2());
        coord[0]/= n;
        coord[1]/= n;
        coord[2]/= n;
        return n;
    }

    void print(){
        cout << "x: " << coord[0] << " y: " << coord[1] << " z: " << coord[2] << endl;
    }
	double coord[3];
};

Vector operator+(const Vector& a, const Vector& b) {
	return Vector(a[0] + b[0], a[1] + b[1], a[2] + b[2]);
}
Vector operator-(const Vector& a, const Vector& b) {
	return Vector(a[0] - b[0], a[1] - b[1], a[2] - b[2]);
}
Vector operator*(const Vector& a, double b) {
	return Vector(a[0]*b, a[1]*b, a[2]*b);
}
Vector operator*(double a, const Vector& b) {
	return Vector(a*b[0], a*b[1], a*b[2]);
}
Vector operator/(
    Vector& a, double b){
    return Vector(a[0]/b, a[1]/b, a[2]/b);
}

double dot(const Vector& a, const Vector& b) {
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

class Ray{
    public:
        Ray(Vector& origin, Vector& direction){
            this->origin = origin;
            this->direction = direction;
        }

        Vector origin;
        Vector direction;
};

class Sphere{
    public:
        Sphere(Vector position, Vector albedo, double radius, bool diffuse = true){
            this->position = position;
            this->albedo = albedo;
            this->radius = radius;
            this->diffuse = diffuse;
        }
        float intersection(Ray ray){
            double b = 2*dot(ray.direction, ray.origin - this->position);
            double a = (ray.direction).norm2();
            double c = (ray.origin - this->position).norm2() - sqr(radius);
            double delta = sqr(b) - 4*a*c;
            if(delta >=0){
                double sqrtDelta = sqrt(delta);
                double t1 = (-b-sqrtDelta)/(2*a);
                double t2 = (-b+sqrtDelta)/(2*a);
                double t;
                if(t1 >=0 && t1 <= t2){
                    t = t1;
                }
                else if (t2 >=0 && t2 <= t1){
                    t = t2;
                }
                if(t1 >=0 || t2 >=0){
                    return t;
                }
                else{
                    return -1;
                }
            }
            return -1;
        }

        Vector position;
        Vector albedo;
        double radius;
        bool diffuse;
};

class Camera{
    public: 
        Camera(Vector position, double fov, int width, int height) : 
            position(position), 
            fov(fov), 
            width(width), 
            height(height), 
            image(width*height*3, 0){};

        Vector position;
        double fov;
        int width;
        int height;
        vector<unsigned char> image;
};

class Light{
    public: 
        Light(Vector position, double I){
            this->position = position;
            this->I = I;
        };
        Vector position; 
        double I;
};

class Scene{
    public:
        Scene(Camera camera, Light light): camera(camera), light(light){};

        void addSphere(Sphere sphere){
            spheres.push_back(sphere);
        };
        
        void intersect(Ray ray, float& t, int& index){
            t = -1;
            index = -1;
            for (unsigned  sphereIndex = 0; sphereIndex < spheres.size(); sphereIndex++){
                Sphere sphere = spheres.at(sphereIndex);
                float t2 = sphere.intersection(ray);
                if((t2 >= 0 && t2 <= t) || t == -1){
                    t = t2;
                    index = sphereIndex;
                }
            }
        };

        Vector getColor(const Ray &ray, int numRebond){
            Vector noir(0,0,0);
            if(numRebond < 0){
                return noir;
            }
            float t;
            int firstSphere;
            intersect(ray, t, firstSphere);
            if(t >= 0){
                Sphere renderedSphere = spheres.at(firstSphere);
                Vector P = ray.origin + t * ray.direction;
                Vector N = (P - renderedSphere.position);
                N.normalize();

                if(renderedSphere.diffuse){
                    return getShadow(P, N, renderedSphere);
                }
                Vector newDirection = ray.direction - 2*dot(ray.direction, N)*N;
                Vector Peps = P + epsilon * N;
                Ray newRay(Peps, newDirection);
                return getColor(newRay, numRebond - 1);
            };
            return noir;
        }

        Vector getShadow(Vector &point, Vector &normal, Sphere &renderedSphere){
            Vector wi = (light.position - point);
            double d = wi.normalize();

            Vector Peps = point + epsilon * normal;
            Ray lightRay(Peps, wi);
            const float lightIntersection = renderedSphere.intersection(lightRay);
            bool visible = lightIntersection == -1;
            bool shadowed = false;

            if(visible){
                float otherSphereIntersection;
                int i;
                intersect(lightRay, otherSphereIntersection, i);
                if(otherSphereIntersection > 0 && sqr(otherSphereIntersection)*lightRay.direction.norm2() < sqr(d)){
                    shadowed = true;
                }
            }

            if(!visible || shadowed){
                return Vector(0,0,0);
            }
            double const c = light.I/(4*sqr(3.14159)*sqr(d))*dot(normal, wi);
            Vector colors = c* (renderedSphere.albedo);
            return Vector(
                min(int(pow(colors[0], 1/2.2)), 255),
                min(int(pow(colors[1], 1/2.2)), 255),
                min(int(pow(colors[2], 1/2.2)), 255));
        }

        void render(){
            #pragma omp parallel for
            for (int i = 0; i < camera.height; i++) {
                for (int j = 0; j < camera.width; j++) {

                    Vector pixel(j-camera.width/2 + 0.5, -i +camera.height/2 - 0.5, -camera.width/(2*tan(camera.fov/2)));
                    pixel.normalize();
                    Ray ray(camera.position, pixel);
                    Vector color = getColor(ray, 5);

                    camera.image[(i*camera.width + j) * 3 + 0] = color[0];   // RED
                    camera.image[(i*camera.width + j) * 3 + 1] = color[1];  // GREEN
                    camera.image[(i*camera.width + j) * 3 + 2] = color[2];  // BLUE
                    
                }
            }
            stbi_write_png("image.png", camera.width, camera.height, 3, &(camera.image[0]), 0);
        };
    private:
        vector<Sphere> spheres;
        Camera camera;
        Light light;
        const float epsilon = 0.001;
};

int main() {
    double pi = 3.14159;
    Sphere sphere(Vector(15,0,0), Vector(0.5,0.2,0.5), 10, false);
    Sphere sphere2(Vector(-15,0,0), Vector(0.5,0.2,0.5), 10, false);
    Sphere wall1(Vector(0,0,-1000), Vector(0.2,0.5,0.1), 940);
    Sphere wall2(Vector(0,1000,0), Vector(0.5,0.2,0.1), 940);
    Sphere wall3(Vector(0,0,1000), Vector(0.5,0.3,0.2), 940);
    Sphere wall4(Vector(0,-1000,0), Vector(0.1,0.2,0.5), 990);
    Sphere wall5(Vector(1000,0,0), Vector(0.6,0.2,0.5), 940);
    Sphere wall6(Vector(-1000,0,0), Vector(0.1,0.5,0.5), 940);
    Camera camera(Vector(0,0,55), pi/3, 1024, 1024);
    Light light(Vector(-10, 20, 40), 2E10);

    Scene scene(camera, light);
    scene.addSphere(sphere);
    scene.addSphere(sphere2);
    scene.addSphere(wall1);
    scene.addSphere(wall2);
    scene.addSphere(wall3);
    scene.addSphere(wall4);
    scene.addSphere(wall5);
    scene.addSphere(wall6);
    scene.render();

    return 0;
}