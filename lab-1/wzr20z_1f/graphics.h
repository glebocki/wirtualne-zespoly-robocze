#include <gl\gl.h>
#include <gl\glu.h>

#ifndef _VEKTOR3D__H 
#include "vector3D.h"
#endif

enum GLDisplayListNames
{
	Wall1 = 1,
	Wall2 = 2,
	Floor = 3,
	Cube = 4,
	Auto = 5,
	EnvironmentMap = 10

};


struct ViewParams
{
	// Parametry widoku:
	Vector3 cam_direct_1,                  // looking direction
		cam_pos_1,                         // camera position
		cam_vertical_1,                    // camera vertical directon       
		cam_direct_2,                      // 
		cam_pos_2,                         //    the same from top view
		cam_vertical_2,                    //  
		cam_direct, cam_pos, cam_vertical; // auxilary variables
	bool tracking;                         // object tracking mode
	bool top_view;                         // top view mode
	float cam_distance;                    // camera distance back to looking direction 
	float cam_angle;                       // 
	float cam_distance_1, cam_angle_1, cam_distance_2, cam_angle_2,
		cam_distance_3, cam_angle_3;
};

int GraphicsInitialisation(HDC g_context);
void DrawScene();
void WindowResize(int cx, int cy);
void EndOfGraphics();

BOOL SetWindowPixelFormat(HDC hDC);
BOOL CreateViewGLContext(HDC hDC);
GLvoid BuildFont(HDC hDC);
GLvoid glPrint(const char *fmt, ...);