/************************************************************
					 Grafika OpenGL
*************************************************************/
#include <windows.h>
#include <gl\gl.h>
#include <gl\glu.h>
#include <iterator> 
#include <map>

using namespace std;

#include "graphics.h"
#include "objects.h"

ViewParams viewpar;

extern MovableObject *my_vehicle;               // obiekt przypisany do tej aplikacji
extern map<int, MovableObject*> movable_objects;

extern Environment env;

int g_GLPixelIndex = 0;
HGLRC g_hGLContext = NULL;
unsigned int font_base;


extern void CreateDisplayLists();		// definiujemy listy tworz¹ce labirynt
extern void DrawGlobalCoordAxes();


int GraphicsInitialisation(HDC g_context)
{

	if (SetWindowPixelFormat(g_context) == FALSE)
		return FALSE;

	if (CreateViewGLContext(g_context) == FALSE)
		return 0;
	BuildFont(g_context);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);


	CreateDisplayLists();		// definiujemy listy tworz¹ce ró¿ne elementy sceny
	env.DrawInitialisation();

	// pocz¹tkowe ustawienia widoku:
	// Parametry widoku:
	viewpar.cam_direct_1 = Vector3(10, -3, -14);   // kierunek patrzenia
	viewpar.cam_pos_1 = Vector3(-35, 6, 10);         // po³o¿enie kamery
	viewpar.cam_vertical_1 = Vector3(0, 1, 0);           // kierunek pionu kamery        
	viewpar.cam_direct_2 = Vector3(0, -1, 0.02);   // to samo dla widoku z góry
	viewpar.cam_pos_2 = Vector3(0, 100, 0);
	viewpar.cam_vertical_2 = Vector3(0, 0, -1);
	viewpar.cam_direct = viewpar.cam_direct_1;
	viewpar.cam_pos = viewpar.cam_pos_1;
	viewpar.cam_vertical = viewpar.cam_vertical_1;
	viewpar.tracking = 0;                             // tryb œledzenia obiektu przez kamerê
	viewpar.top_view = 0;                          // tryb widoku z góry
	viewpar.cam_distance = 18.0;                          // cam_distance widoku z kamery
	viewpar.cam_angle = 0;                            // obrót kamery góra-dó³
	viewpar.cam_distance_1 = viewpar.cam_distance;
	viewpar.cam_angle_1 = viewpar.cam_angle;
	viewpar.cam_distance_2 = viewpar.cam_distance;
	viewpar.cam_angle_2 = viewpar.cam_angle;
	viewpar.cam_distance_3 = viewpar.cam_distance;
	viewpar.cam_angle_3 = viewpar.cam_angle;
}


void DrawScene()
{
	GLfloat BlueSurface[] = { 0.4f, 0.0f, 0.8f, 0.5f };
	GLfloat DGreenSurface[] = { 0.2f, 0.8f, 0.35f, 0.7f };
	GLfloat RedSurface[] = { 0.8f, 0.2f, 0.1f, 0.5f };
	GLfloat GreenSurface[] = { 0.5f, 0.8f, 0.3f, 1.0f };
	GLfloat YellowSurface[] = { 0.75f, 0.75f, 0.0f, 1.0f };
	GLfloat LYellowSurface[] = { 10.0f, 10.0f, 8.0f, 1.0f };

	GLfloat LightAmbient[] = { 0.1f, 0.1f, 0.1f, 0.1f };
	GLfloat LightDiffuse[] = { 0.2f, 0.7f, 0.7f, 0.7f };
	GLfloat LightPosition[] = { 5.0f, 5.0f, 5.0f, 0.0f };

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glLightfv(GL_LIGHT0, GL_AMBIENT, LightAmbient);		//1 sk³adowa: œwiat³o otaczaj¹ce (bezkierunkowe)
	glLightfv(GL_LIGHT0, GL_DIFFUSE, LightDiffuse);		//2 sk³adowa: œwiat³o rozproszone (kierunkowe)
	glLightfv(GL_LIGHT0, GL_POSITION, LightPosition);
	glEnable(GL_LIGHT0);

	glPushMatrix();
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, BlueSurface);

	glClearColor(0.05, 0.05, 0.1, 0.7);

	Vector3 direction_k, vertical_k, position_k;
	if (viewpar.tracking)
	{
		direction_k = my_vehicle->state.qOrient.rotate_vector(Vector3(1, 0, 0));
		vertical_k = my_vehicle->state.qOrient.rotate_vector(Vector3(0, 1, 0));
		Vector3 v_cam_right = my_vehicle->state.qOrient.rotate_vector(Vector3(0, 0, 1));

		vertical_k = vertical_k.rotation(viewpar.cam_angle, v_cam_right.x, v_cam_right.y, v_cam_right.z);
		direction_k = direction_k.rotation(viewpar.cam_angle, v_cam_right.x, v_cam_right.y, v_cam_right.z);
		position_k = my_vehicle->state.vPos - direction_k*my_vehicle->length * 0 +
			vertical_k.znorm()*my_vehicle->height * 5;
		viewpar.cam_vertical = vertical_k;
		viewpar.cam_direct = direction_k;
		viewpar.cam_pos = position_k;
	}
	else
	{
		vertical_k = viewpar.cam_vertical;
		direction_k = viewpar.cam_direct;
		position_k = viewpar.cam_pos;
		Vector3 v_cam_right = (direction_k*vertical_k).znorm();
		vertical_k = vertical_k.rotation(viewpar.cam_angle / 20, v_cam_right.x, v_cam_right.y, v_cam_right.z);
		direction_k = direction_k.rotation(viewpar.cam_angle / 20, v_cam_right.x, v_cam_right.y, v_cam_right.z);
	}

	// Ustawianie widoku sceny    
	gluLookAt(position_k.x - viewpar.cam_distance*direction_k.x,
		position_k.y - viewpar.cam_distance*direction_k.y, position_k.z - viewpar.cam_distance*direction_k.z,
		position_k.x + direction_k.x, position_k.y + direction_k.y, position_k.z + direction_k.z,
		vertical_k.x, vertical_k.y, vertical_k.z);

	//glRasterPos2f(0.30,-0.27);
	//glPrint("my_vehicle->iID = %d",my_vehicle->iID ); 

	DrawGlobalCoordAxes();

	//glPushMatrix();
	for (int w = -1; w < 2; w++)
		for (int k = -1; k < 2; k++)
		{
			glPushMatrix();

			glTranslatef(env.number_of_columns*env.field_size*k, 0, env.number_of_rows*env.field_size*w);

			glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, BlueSurface);
			glEnable(GL_BLEND);

			my_vehicle->DrawObject();

			for (map<int, MovableObject*>::iterator it = movable_objects.begin(); it != movable_objects.end(); it++)
			{
				glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, DGreenSurface);
				it->second->DrawObject();
			}

			glDisable(GL_BLEND);
			glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, GreenSurface);

			env.Draw();
			glPopMatrix();
		}

	// s³oneczko:
	GLUquadricObj *Qdisk = gluNewQuadric();
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, LYellowSurface);
	glTranslatef(my_vehicle->state.vPos.x, my_vehicle->state.vPos.y + 400, my_vehicle->state.vPos.z + 4000);

	//glTranslatef(0,300,5000);
	gluDisk(Qdisk, 0, 200, 40, 40);
	gluDeleteQuadric(Qdisk);

	glPopMatrix();

	glFlush();

}

void WindowResize(int cx, int cy)
{
	GLsizei width, height;
	GLdouble aspect;
	width = cx;
	height = cy;

	if (cy == 0)
		aspect = (GLdouble)width;
	else
		aspect = (GLdouble)width / (GLdouble)height;

	glViewport(0, 0, width, height);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(35, aspect, 1, 10000.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glDrawBuffer(GL_BACK);

	glEnable(GL_LIGHTING);

	glEnable(GL_DEPTH_TEST);

}


void EndOfGraphics()
{
	if (wglGetCurrentContext() != NULL)
	{
		// dezaktualizacja contextu renderuj¹cego
		wglMakeCurrent(NULL, NULL);
	}
	if (g_hGLContext != NULL)
	{
		wglDeleteContext(g_hGLContext);
		g_hGLContext = NULL;
	}
	glDeleteLists(font_base, 96);
}

BOOL SetWindowPixelFormat(HDC hDC)
{
	PIXELFORMATDESCRIPTOR pixelDesc;

	pixelDesc.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pixelDesc.nVersion = 1;
	pixelDesc.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_STEREO_DONTCARE;
	pixelDesc.iPixelType = PFD_TYPE_RGBA;
	pixelDesc.cColorBits = 32;
	pixelDesc.cRedBits = 8;
	pixelDesc.cRedShift = 16;
	pixelDesc.cGreenBits = 8;
	pixelDesc.cGreenShift = 8;
	pixelDesc.cBlueBits = 8;
	pixelDesc.cBlueShift = 0;
	pixelDesc.cAlphaBits = 0;
	pixelDesc.cAlphaShift = 0;
	pixelDesc.cAccumBits = 64;
	pixelDesc.cAccumRedBits = 16;
	pixelDesc.cAccumGreenBits = 16;
	pixelDesc.cAccumBlueBits = 16;
	pixelDesc.cAccumAlphaBits = 0;
	pixelDesc.cDepthBits = 32;
	pixelDesc.cStencilBits = 8;
	pixelDesc.cAuxBuffers = 0;
	pixelDesc.iLayerType = PFD_MAIN_PLANE;
	pixelDesc.bReserved = 0;
	pixelDesc.dwLayerMask = 0;
	pixelDesc.dwVisibleMask = 0;
	pixelDesc.dwDamageMask = 0;
	g_GLPixelIndex = ChoosePixelFormat(hDC, &pixelDesc);

	if (g_GLPixelIndex == 0)
	{
		g_GLPixelIndex = 1;

		if (DescribePixelFormat(hDC, g_GLPixelIndex, sizeof(PIXELFORMATDESCRIPTOR), &pixelDesc) == 0)
		{
			return FALSE;
		}
	}

	if (SetPixelFormat(hDC, g_GLPixelIndex, &pixelDesc) == FALSE)
	{
		return FALSE;
	}

	return TRUE;
}
BOOL CreateViewGLContext(HDC hDC)
{
	g_hGLContext = wglCreateContext(hDC);

	if (g_hGLContext == NULL)
	{
		return FALSE;
	}

	if (wglMakeCurrent(hDC, g_hGLContext) == FALSE)
	{
		return FALSE;
	}

	return TRUE;
}

GLvoid BuildFont(HDC hDC)								// Build Our Bitmap Font
{
	HFONT	font;										// Windows Font ID
	HFONT	oldfont;									// Used For Good House Keeping

	font_base = glGenLists(96);								// Storage For 96 Characters

	font = CreateFont(-14,							// Height Of Font
		0,								// Width Of Font
		0,								// Angle Of Escapement
		0,								// Orientation Angle
		FW_NORMAL,						// Font Weight
		FALSE,							// Italic
		FALSE,							// Underline
		FALSE,							// Strikeout
		ANSI_CHARSET,					// Character Set Identifier
		OUT_TT_PRECIS,					// Output Precision
		CLIP_DEFAULT_PRECIS,			// Clipping Precision
		ANTIALIASED_QUALITY,			// Output Quality
		FF_DONTCARE | DEFAULT_PITCH,		// Family And Pitch
		"Courier New");					// Font Name

	oldfont = (HFONT)SelectObject(hDC, font);           // Selects The Font We Want
	wglUseFontBitmaps(hDC, 32, 96, font_base);				// Builds 96 Characters Starting At Character 32
	SelectObject(hDC, oldfont);							// Selects The Font We Want
	DeleteObject(font);									// Delete The Font
}

// Napisy w OpenGL
GLvoid glPrint(const char *fmt, ...)	// Custom GL "Print" Routine
{
	char		text[256];	// Holds Our String
	va_list		ap;		// Pointer To List Of Arguments

	if (fmt == NULL)		// If There's No Text
		return;			// Do Nothing

	va_start(ap, fmt);		// Parses The String For Variables
	vsprintf(text, fmt, ap);	// And Converts Symbols To Actual Numbers
	va_end(ap);			// Results Are Stored In Text

	glPushAttrib(GL_LIST_BIT);	// Pushes The Display List Bits
	glListBase(font_base - 32);		// Sets The Base Character to 32
	glCallLists(strlen(text), GL_UNSIGNED_BYTE, text);	// Draws The Display List Text
	glPopAttrib();			// Pops The Display List Bits
}


void CreateDisplayLists()
{
	glNewList(Wall1, GL_COMPILE);	// GL_COMPILE - lista jest kompilowana, ale nie wykonywana

	glBegin(GL_QUADS);		// inne opcje: GL_POINTS, GL_LINES, GL_LINE_STRIP, GL_LINE_LOOP
	// GL_TRIANGLES, GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN, GL_QUAD_STRIP, GL_POLYGON
	glNormal3f(-1.0, 0.0, 0.0);
	glVertex3f(-1.0, -1.0, 1.0);
	glVertex3f(-1.0, 1.0, 1.0);
	glVertex3f(-1.0, 1.0, -1.0);
	glVertex3f(-1.0, -1.0, -1.0);
	glEnd();
	glEndList();

	glNewList(Wall2, GL_COMPILE);
	glBegin(GL_QUADS);
	glNormal3f(1.0, 0.0, 0.0);
	glVertex3f(1.0, -1.0, 1.0);
	glVertex3f(1.0, 1.0, 1.0);
	glVertex3f(1.0, 1.0, -1.0);
	glVertex3f(1.0, -1.0, -1.0);
	glEnd();
	glEndList();

	glNewList(Auto, GL_COMPILE);
	glBegin(GL_QUADS);
	// przod
	glNormal3f(0.0, 0.0, 1.0);

	glVertex3f(0, 0, 1);
	glVertex3f(0, 1, 1);
	glVertex3f(0.7, 1, 1);
	glVertex3f(0.7, 0, 1);

	glVertex3f(0.7, 0, 1);
	glVertex3f(0.7, 0.5, 1);
	glVertex3f(1.0, 0.5, 1);
	glVertex3f(1.0, 0, 1);
	// tyl
	glNormal3f(0.0, 0.0, -1.0);
	glVertex3f(0, 0, 0);
	glVertex3f(0.7, 0, 0);
	glVertex3f(0.7, 1, 0);
	glVertex3f(0, 1, 0);

	glVertex3f(0.7, 0, 0);
	glVertex3f(1.0, 0, 0);
	glVertex3f(1.0, 0.5, 0);
	glVertex3f(0.7, 0.5, 0);
	// gora
	glNormal3f(0.0, 1.0, 0.0);
	glVertex3f(0, 1, 0);
	glVertex3f(0, 1, 1);
	glVertex3f(0.7, 1, 1);
	glVertex3f(0.7, 1, 0);

	glVertex3f(0.7, 0.5, 0);
	glVertex3f(0.7, 0.5, 1);
	glVertex3f(1.0, 0.5, 1);
	glVertex3f(1.0, 0.5, 0);
	// dol
	glNormal3f(0.0, -1.0, 0.0);
	glVertex3f(0, 0, 0);
	glVertex3f(1, 0, 0);
	glVertex3f(1, 0, 1);
	glVertex3f(0, 0, 1);
	// prawo
	glNormal3f(1.0, 0.0, 0.0);
	glVertex3f(0.7, 0.5, 0);
	glVertex3f(0.7, 0.5, 1);
	glVertex3f(0.7, 1, 1);
	glVertex3f(0.7, 1, 0);

	glVertex3f(1.0, 0.0, 0);
	glVertex3f(1.0, 0.0, 1);
	glVertex3f(1.0, 0.5, 1);
	glVertex3f(1.0, 0.5, 0);
	// lewo
	glNormal3f(-1.0, 0.0, 0.0);
	glVertex3f(0, 0, 0);
	glVertex3f(0, 1, 0);
	glVertex3f(0, 1, 1);
	glVertex3f(0, 0, 1);

	glEnd();
	glEndList();

}


void DrawGlobalCoordAxes(void)
{

	glColor3f(1, 0, 0);
	glBegin(GL_LINES);
	glVertex3f(0, 0, 0);
	glVertex3f(2, 0, 0);
	glVertex3f(2, -0.25, 0.25);
	glVertex3f(2, 0.25, -0.25);
	glVertex3f(2, -0.25, -0.25);
	glVertex3f(2, 0.25, 0.25);

	glEnd();
	glColor3f(0, 1, 0);
	glBegin(GL_LINES);
	glVertex3f(0, 0, 0);
	glVertex3f(0, 2, 0);
	glVertex3f(0, 2, 0);
	glVertex3f(0.25, 2, 0);
	glVertex3f(0, 2, 0);
	glVertex3f(-0.25, 2, 0.25);
	glVertex3f(0, 2, 0);
	glVertex3f(-0.25, 2, -0.25);

	glEnd();
	glColor3f(0, 0, 1);
	glBegin(GL_LINES);
	glVertex3f(0, 0, 0);
	glVertex3f(0, 0, 2);
	glVertex3f(-0.25, -0.25, 2);
	glVertex3f(0.25, 0.25, 2);
	glVertex3f(-0.25, -0.25, 2);
	glVertex3f(0.25, -0.25, 2);
	glVertex3f(-0.25, 0.25, 2);
	glVertex3f(0.25, 0.25, 2);

	glEnd();

	glColor3f(1, 1, 1);
}

