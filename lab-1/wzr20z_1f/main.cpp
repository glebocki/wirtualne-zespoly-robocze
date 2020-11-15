/****************************************************
	Virtual Collaborative Teams - The base program 
    The main module
	****************************************************/

#include <windows.h>
#include <math.h>
#include <time.h>
#include <gl\gl.h>
#include <gl\glu.h>
#include <iterator> 
#include <map>

#include "objects.h"
#include "graphics.h"
#include "net.h"
using namespace std;

FILE *f = fopen("wzr_log.txt", "w");               // log file handle


MovableObject *my_vehicle;                         // vehicle controlled in this application
Environment env;                                   // environment: terrain, things (trees, coins etc.)

map<int, MovableObject*> movable_objects;          // objects from other applications indexed by iID  

float cycle_time;                                         // average time between two consecutive cycles of world (simulation + rendering)
                                                   // is used to synchronize simulated phisical time in each application despite of
                                                   // hardware or processes number differences between computers 
long time_of_cycle, number_of_cyc;                 // variables supported cycle_time calculation

multicast_net *multi_reciv;						   // object (see net module) to recive messages from other applications
multicast_net *multi_send;                         // ...  to send messages ...

HWND main_window;                                  // a handle to main window of application
HANDLE threadReciv;                                // a handle of thread for reciving messages and actions execution depend on message type 
bool if_SHIFT_pressed = false;
bool if_ID_visible = true;                         
bool if_mouse_control = false;                     // vehicle control by mouse
int mouse_cursor_x = 0, mouse_cursor_y = 0;        // position of mouse cursor

extern ViewParams viewpar;                         // view parameters (camera position, camera angle, etc)


struct Frame                                       // The main structure of net communication between aplications. Homogenous for simpicity.
{
	int type;                                      // frame type  
	int iID;                                       // object identifier
	ObjectState state;                             // object state values (see object module)
	int changeiID;
};


//******************************************************
// The function of handling the message receiving thread
DWORD WINAPI ReceiveThreadFun(void *ptr)
{
	multicast_net *pmt_net = (multicast_net*)ptr;  // the pointer to the object of multicast_net class (see net module)
	Frame frame, changeFrame;

	while (1)
	{
		int frame_size = pmt_net->reciv((char*)&frame, sizeof(Frame));   // waiting for frame 
		ObjectState state = frame.state;

		//fprintf(f, "my iID = %d, received state of object with iID = %d\n", my_vehicle->iID, frame.iID);

		if (frame.iID != my_vehicle->iID)                     // if the frame isn't my own frame
		{
			if (movable_objects[frame.iID] == NULL)           // object hasn't registered up till now 
				                        // == nullptr (C++ 11) it is not available in older versions than VC 2013
			{
				MovableObject *ob = new MovableObject();
				ob->iID = frame.iID;
				movable_objects[frame.iID] = ob;              // registration of new object 
		         
				//fprintf(f, "alien object ID = %d was registred\n", ob->iID);
			}

			movable_objects[frame.iID]->ChangeState(state);   // updating the state of the object

			if (frame.type == 1 && my_vehicle->changeID == frame.iID && frame.changeiID == my_vehicle->iID) {
				changeFrame.state = my_vehicle->State();                // state of my own object
				changeFrame.iID = my_vehicle->iID;                      // my object identifier
				changeFrame.type = 2;
				changeFrame.changeiID = my_vehicle->iID;

				multi_send->send((char*)&changeFrame, sizeof(Frame));   // sending a message to other application

				my_vehicle->ChangeState(state);
				my_vehicle->changeID = -1;
			}
			if (frame.type == 2 && my_vehicle->changeID == frame.iID && frame.changeiID == my_vehicle->changeID) {
				my_vehicle->ChangeState(state);
				my_vehicle->changeID = -1;
			}

		}
	}  // while(1)
	return 1;
}

// *******************************************************************************
// ****    All events which should be initialized once at the start of application  
void InteractionInitialisation()
{
	DWORD dwThreadId;

	my_vehicle = new MovableObject();    // creating my own object 

	time_of_cycle = clock();             // current time

	// net multicast communication objects with virtual collaborative teams IP and port number
	multi_reciv = new multicast_net("224.12.12.110", 10001);      // object for receiving messages
	multi_send = new multicast_net("224.12.12.110", 10001);       // object for sending messages


	// creating a thread to handling messages from other aplications
	threadReciv = CreateThread(
		NULL,                            // no security attributes
		0,                               // use default stack size
		ReceiveThreadFun,                // thread function
		(void *)multi_reciv,             // argument to thread function
		NULL,                            // use default creation flags
		&dwThreadId);                    // returns the thread identifier

}


// *****************************************************************
// ****    All things (without graphics) to do as frequent as possible due to world cycle
void VirtualWorldCycle()
{
	number_of_cyc++;

	// average time - cycle_time between two consequtive world cycles calculation: 
	if (number_of_cyc % 50 == 0)         
	{                              
		char text[256];
		long prev_time = time_of_cycle;
		time_of_cycle = clock();
		float fFps = (50 * CLOCKS_PER_SEC) / (float)(time_of_cycle - prev_time);
		if (fFps != 0) cycle_time = 1.0 / fFps; else cycle_time = 1;

		sprintf(text, "WZR-lab 2020/21 temat 1, wer. f (%0.0f fps  %0.2fms) ", fFps, 1000.0 / fFps);
		SetWindowText(main_window, text); 			
	}

	my_vehicle->Simulation(cycle_time);                      // simulation of own object based of previous state, forces and 
	                                                  // other actions and cycle_time - average time elapsed since the last simulation
	Frame frame;
	frame.state = my_vehicle->State();                // state of my own object
	frame.iID = my_vehicle->iID;                      // my object identifier
	frame.type = 0;
	frame.changeiID = my_vehicle->changeID;

	multi_send->send((char*)&frame, sizeof(Frame));   // sending a message to other application
}


//******************************************************************
// Funkcja wysylajaca zadanie zamiany stanu do innego obiektu
void sendChangeRequest(int changeID) {
	my_vehicle->changeID = changeID;
	fprintf(f, "my iID = %d, whant to change with iID = %d\n", my_vehicle->iID, changeID);
	Frame frame;
	frame.state = my_vehicle->State();                // state of my own object
	frame.iID = my_vehicle->iID;                      // my object identifier
	frame.type = 1;
	frame.changeiID = my_vehicle->changeID;

	multi_send->send((char*)&frame, sizeof(Frame));   // sending a message to other application
}

// *****************************************************************
// ****    All things to do at the end of aplication
void EndOfInteraction()
{
	fprintf(f, "End of interaction\n");
	fclose(f);
}

// window messages handling function - the prototype
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);


HDC g_context = NULL;               // a handel of graphical context



//Window main thread function
int WINAPI WinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR     lpCmdLine,
	int       nCmdShow)
{
	MSG message;		  
	WNDCLASS main_class; 

	static char class_name[] = "Main Class";

	main_class.style = CS_HREDRAW | CS_VREDRAW;
	main_class.lpfnWndProc = WndProc; 
	main_class.cbClsExtra = 0;
	main_class.cbWndExtra = 0;
	main_class.hInstance = hInstance; 
	main_class.hIcon = 0;
	main_class.hCursor = LoadCursor(0, IDC_ARROW);
	main_class.hbrBackground = (HBRUSH)GetStockObject(GRAY_BRUSH);
	main_class.lpszMenuName = "Menu";
	main_class.lpszClassName = class_name;

	RegisterClass(&main_class);

	main_window = CreateWindow(class_name, "WZR-lab 2020/21 topic 1 - ver. f", WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
		100, 50, 600, 400, NULL, NULL, hInstance, NULL);

	//main_window = CreateWindow(class_name, "WZR-lab 2020/21 topic 1 - ver. f", WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
		//100, 50, 1200, 800, NULL, NULL, hInstance, NULL);

	ShowWindow(main_window, nCmdShow);

	UpdateWindow(main_window);

	ZeroMemory(&message, sizeof(message));
	while (message.message != WM_QUIT)
	{
		if (PeekMessage(&message, NULL, 0U, 0U, PM_REMOVE))
		{
			TranslateMessage(&message);
			DispatchMessage(&message);
		}
		else
		{
			VirtualWorldCycle();    
			InvalidateRect(main_window, NULL, FALSE);
		}
	}

	return (int)message.wParam;
}

//********************************************************************
// Operating system message handling function:
LRESULT CALLBACK WndProc(HWND main_window, UINT message_code, WPARAM wParam, LPARAM lParam)
{

	switch (message_code)
	{
	case WM_CREATE:  // message which is sent when window is creating
	{

		g_context = GetDC(main_window);

		srand((unsigned)time(NULL));
		int result = GraphicsInitialisation(g_context);
		if (result == 0)
		{
			printf("cannot open graphical window!\n");
			//exit(1);
		}

		InteractionInitialisation();

		SetTimer(main_window, 1, 10, NULL);

		return 0;
	}


	case WM_PAINT:
	{
		PAINTSTRUCT paint;
		HDC context;
		context = BeginPaint(main_window, &paint);

		DrawScene();
		SwapBuffers(context);

		EndPaint(main_window, &paint);

		return 0;
	}

	case WM_TIMER:

		return 0;

	case WM_SIZE:
	{
		int cx = LOWORD(lParam);
		int cy = HIWORD(lParam);

		WindowResize(cx, cy);

		return 0;
	}

	case WM_DESTROY:    // window close message 

		EndOfInteraction();
		EndOfGraphics();

		ReleaseDC(main_window, g_context);
		KillTimer(main_window, 1);

		PostQuitMessage(0);
		return 0;

	case WM_LBUTTONDOWN: // mouse left button press message 
	{
		int x = LOWORD(lParam);
		int y = HIWORD(lParam);
		if (if_mouse_control)
			my_vehicle->F = 30.0;        // the force pushing vehicle forward
		break;
	}
	case WM_RBUTTONDOWN: 
	{
		int x = LOWORD(lParam);
		int y = HIWORD(lParam);
		if (if_mouse_control)
			my_vehicle->F = -5.0;        // the force pushing vehicle backward
		break;
	}
	case WM_MBUTTONDOWN: 
	{
		if_mouse_control = 1 - if_mouse_control;
		if (if_mouse_control) my_vehicle->if_keep_steer_wheel = true;
		else my_vehicle->if_keep_steer_wheel = false;

		mouse_cursor_x = LOWORD(lParam);
		mouse_cursor_y = HIWORD(lParam);
		break;
	}
	case WM_LBUTTONUP: 
	{
		if (if_mouse_control)
			my_vehicle->F = 0.0;        
		break;
	}
	case WM_RBUTTONUP: 
	{
		if (if_mouse_control)
			my_vehicle->F = 0.0;       
		break;
	}
	case WM_MOUSEMOVE:
	{
		int x = LOWORD(lParam);
		int y = HIWORD(lParam);
		if (if_mouse_control)
		{
			float wheel_angle = (float)(mouse_cursor_x - x) / 20;
			if (wheel_angle > 60) wheel_angle = 60;
			if (wheel_angle < -60) wheel_angle = -60;
			my_vehicle->state.steering_angle = PI*wheel_angle / 180;
		}
		break;
	}
	case WM_KEYDOWN:
	{

		switch (LOWORD(wParam))
		{
		case VK_SHIFT:
		{
			if_SHIFT_pressed = 1;
			break;
		}
		case VK_SPACE:
		{
			my_vehicle->breaking_factor = 1.0;       // breaking degree factor
			break;                                   // if 1.0 then wheels are blocked 
		}
		case VK_UP:
		{
			my_vehicle->F = 50.0;                    // the force pushing vehicle forward
			break;
		}
		case VK_DOWN:
		{
			my_vehicle->F = -20.0;
			break;
		}
		case VK_LEFT:
		{
			if (my_vehicle->steer_wheel_speed < 0){
				my_vehicle->steer_wheel_speed = 0;
				my_vehicle->if_keep_steer_wheel = true;
			}
			else{
				if (if_SHIFT_pressed) my_vehicle->steer_wheel_speed = 0.5;
				else my_vehicle->steer_wheel_speed = 0.25 / 8;
			}

			break;
		}
		case VK_RIGHT:
		{
			if (my_vehicle->steer_wheel_speed > 0){
				my_vehicle->steer_wheel_speed = 0;
				my_vehicle->if_keep_steer_wheel = true;
			}
			else{
				if (if_SHIFT_pressed) my_vehicle->steer_wheel_speed = -0.5;
				else my_vehicle->steer_wheel_speed = -0.25 / 8;
			}
			break;
		}
		case 'I':   
		{
			if_ID_visible = 1 - if_ID_visible;
			break;
		}
		case 'W':  // far view
		{
			if (viewpar.cam_distance > 0.5) viewpar.cam_distance /= 1.2;
			else viewpar.cam_distance = 0;
			break;
		}
		case 'S':   // near viev
		{
			if (viewpar.cam_distance > 0) viewpar.cam_distance *= 1.2;
			else viewpar.cam_distance = 0.5;
			break;
		}
		case 'Q':   // top view 
		{
			if (viewpar.tracking) break;
			viewpar.top_view = 1 - viewpar.top_view;
			if (viewpar.top_view)
			{
				viewpar.cam_pos_1 = viewpar.cam_pos; viewpar.cam_direct_1 = viewpar.cam_direct; viewpar.cam_vertical_1 = viewpar.cam_vertical;
				viewpar.cam_distance_1 = viewpar.cam_distance; viewpar.cam_angle_1 = viewpar.cam_angle;
				viewpar.cam_pos = viewpar.cam_pos_2; viewpar.cam_direct = viewpar.cam_direct_2; viewpar.cam_vertical = viewpar.cam_vertical_2;
				viewpar.cam_distance = viewpar.cam_distance_2; viewpar.cam_angle = viewpar.cam_angle_2;
			}
			else
			{
				viewpar.cam_pos_2 = viewpar.cam_pos; viewpar.cam_direct_2 = viewpar.cam_direct; viewpar.cam_vertical_2 = viewpar.cam_vertical;
				viewpar.cam_distance_2 = viewpar.cam_distance; viewpar.cam_angle_2 = viewpar.cam_angle;
				viewpar.cam_pos = viewpar.cam_pos_1; viewpar.cam_direct = viewpar.cam_direct_1; viewpar.cam_vertical = viewpar.cam_vertical_1;
				viewpar.cam_distance = viewpar.cam_distance_1; viewpar.cam_angle = viewpar.cam_angle_1;
			}
			break;
		}
		case 'E':   
		{
			viewpar.cam_angle += PI * 5 / 180;
			break;
		}
		case 'D':   
		{
			viewpar.cam_angle -= PI * 5 / 180;
			break;
		}
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		{
			int changeID = (int)LOWORD(wParam) - 48;
			sendChangeRequest(changeID);
			break;
		}
		case 'A':   // switching on/off object tracking
		{
			viewpar.tracking = 1 - viewpar.tracking;
			if (viewpar.tracking)
			{
				viewpar.cam_distance = viewpar.cam_distance_3; viewpar.cam_angle = viewpar.cam_angle_3;
			}
			else
			{
				viewpar.cam_distance_3 = viewpar.cam_distance; viewpar.cam_angle_3 = viewpar.cam_angle;
				viewpar.top_view = 0;
				viewpar.cam_pos = viewpar.cam_pos_1; viewpar.cam_direct = viewpar.cam_direct_1; viewpar.cam_vertical = viewpar.cam_vertical_1;
				viewpar.cam_distance = viewpar.cam_distance_1; viewpar.cam_angle = viewpar.cam_angle_1;
			}
			break;
		}
		case VK_F1:  // help page
		{
			char lan[1024], lan_bie[1024];
			GetCurrentDirectory(1024, lan_bie);
			strcpy(lan, "C:\\Program Files\\Internet Explorer\\iexplore ");
			strcat(lan, lan_bie);
			strcat(lan, "\\help.htm");
			int wyni = WinExec(lan, SW_NORMAL);
			if (wyni < 32)  
			{
				strcpy(lan, "C:\\Program Files\\Mozilla Firefox\\firefox ");
				strcat(lan, lan_bie);
				strcat(lan, "\\help.htm");
				wyni = WinExec(lan, SW_NORMAL);
				if (wyni < 32)
				{
					char lan_win[1024];
					GetWindowsDirectory(lan_win, 1024);
					strcat(lan_win, "\\notepad help.txt ");
					wyni = WinExec(lan_win, SW_NORMAL);
				}
			}
			break;
		}
		case VK_ESCAPE:
		{
			SendMessage(main_window, WM_DESTROY, 0, 0);
			break;
		}
		} // switch under keys

		break;
	}
	case WM_KEYUP:
	{
		switch (LOWORD(wParam))
		{
		case VK_SHIFT:
		{
			if_SHIFT_pressed = 0;
			break;
		}
		case VK_SPACE:
		{
			my_vehicle->breaking_factor = 0.0;
			break;
		}
		case VK_UP:
		{
			my_vehicle->F = 0.0;
			break;
		}
		case VK_DOWN:
		{
			my_vehicle->F = 0.0;
			break;
		}
		case VK_LEFT:
		{
			my_vehicle->Fb = 0.00;
			//my_vehicle->state.steering_angle = 0;
			if (my_vehicle->if_keep_steer_wheel) my_vehicle->steer_wheel_speed = -0.25/8;
			else my_vehicle->steer_wheel_speed = 0; 
			my_vehicle->if_keep_steer_wheel = false;
			break;
		}
		case VK_RIGHT:
		{
			my_vehicle->Fb = 0.00;
			//my_vehicle->state.steering_angle = 0;
			if (my_vehicle->if_keep_steer_wheel) my_vehicle->steer_wheel_speed = 0.25 / 8;
			else my_vehicle->steer_wheel_speed = 0;
			my_vehicle->if_keep_steer_wheel = false;
			break;
		}

		}

		break;
	}

	default: 
		return DefWindowProc(main_window, message_code, wParam, lParam);
	}


}

