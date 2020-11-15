#include <stdio.h>
#include "quaternion.h"

#define PI 3.1415926

struct ObjectState
{
	Vector3 vPos;             // vehicle center position (geometric center)
	quaternion qOrient;       // orientation (angular position)
	Vector3 vV, vA;           // vehicle center velocity and acceleration vectors 
	Vector3 vV_ang, vA_ang;   // angular velocity and acceleration vectors 
	float steering_angle;     // front wheels rotation angle  (positive - to the left)
};

class MovableObject
{
public:
	int iID;                  // object identifier

	ObjectState state;

	float F, Fb;                  // F - pushing force (positive if forward, negative if backward)
	float breaking_factor;        // breaking degree Fh_max = friction*Fy*breaking_degree
	float Fy;                     // si³a nacisku na podstawê pojazdu - gdy obiekt styka siê z pod³o¿em (od niej zale¿y si³a hamowania)
	float steer_wheel_speed;      // steering wheel speed [rad/s]
	bool if_keep_steer_wheel;     

	float mass_own;				  // own vehicle mass witout fuel 
	float length, width, height;  // sizes in local axes x,y,z
	float clearance;              // the distanse between ground and vehicle chassis
	float front_axis_dist;        // the distanse from front axis to front of body
	float back_axis_dist;         // the distanse from back axis to back end of body  	
	float steer_wheel_ret_speed;  // steering wheel return speed[rad / s]

public:
	MovableObject();             
	~MovableObject();
	void ChangeState(ObjectState state);          // object state changing
	ObjectState State();          
	void Simulation(float dt);    // function which calculates new state based on
                                  // previous state, time step and forces 
	void DrawObject();			   					
};

class Environment
{
public:
	float **height_map;           // height of corners and centers of square sectors (each sector is composed of 4 triangle)
	float ***d;                   // free term in the plane equation for each triangle
	Vector3 ***Norm;              // normal vectors to triangle planes
	float field_size;             // length of the side of the square sector
	long number_of_rows, number_of_columns;       // numbers of sectors in two dimensions of array which represents map     
	Environment();
	~Environment();
	float HeightOverGround(float x, float z);     // the height of tarrain in point (x,z) 
	void Draw();	                      
	void DrawInitialisation();              
};
