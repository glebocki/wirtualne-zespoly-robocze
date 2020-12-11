/****************************************************
	Virtual Collaborative Teams - The base program 
    The main module
****************************************************/

bool if_prediction_test = false;
bool if_delays = false;
bool if_shadow = true;
// Scenariusz testu predykcji - tzw. benchmark - dzi�ki temu mo�na por�wna� r�ne algorytmy predykcji na tym samym scenariuszu:
// {czas [s], si�a [N], pr�dko�� skr�cania k� [rad/s], stopie� hamowania} -> przez jaki czas obiekt ma sie porusza� z podan� pr�dko�ci� i k�tem skr�tu k�
float test_scenario[][4] = { { 9.5, 100, 0, 0 }, { 5, 20, -0.25 / 8, 0 }, { 0.5, 0, 0, 1.0 }, { 5, 60, 0.25 / 8, 0 }, { 15, 100, 0, 0 } };
//float test_scenario[][4] = { { 9.5, 500, 0, 0 }, { 10, -200, -0.25 / 2, 0 } };  // scenariusz ekstremalny


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

FILE *f = fopen("wzr_log.txt", "a"); // plik do zapisu informacji testowych


MovableObject *my_vehicle;               // obiekt przypisany do tej aplikacji
Terrain terrain;


map<int, MovableObject*> other_vehicles;

float avg_cycle_time;                // sredni czas pomiedzy dwoma kolejnymi cyklami symulacji i wyswietlania
long time_of_cycle, number_of_cyc=0;   // zmienne pomocnicze potrzebne do obliczania avg_cycle_time
long time_start = clock();           // moment uruchomienia aplikacji 
long time_last_send = 0;             // moment wys�ania ostatniej ramki  

multicast_net *multi_reciv;          // wsk do obiektu zajmujacego sie odbiorem komunikatow
multicast_net *multi_send;           //   -||-  wysylaniem komunikatow

HANDLE threadReciv;                  // uchwyt w�tku odbioru komunikat�w
HWND window_handle;                    // uchwyt do g��wnego okna programu 
CRITICAL_SECTION m_cs;               // do synchronizacji w�tk�w

bool if_SHIFT_pressed = false;
bool if_ID_visible = true;           // czy rysowac nr ID przy ka�dym obiekcie
bool if_mouse_control = false;       // sterowanie za pomoc� klawisza myszki
int mouse_cursor_x = 0, mouse_cursor_y = 0;     // po�o�enie kursora myszy

extern ViewParams view_parameters;           // ustawienia widoku zdefiniowane w grafice

long time_of_day = 1000;         // czas trwania dnia w [s]

// zmienne zwi�zane z nawigacj� obliczeniow�:
long number_of_send_trials = 0;        // liczba pr�b wysylania ramki ze stanem  
float sum_differences_of_pos = 0;             // sumaryczna odleg�o�� pomi�dzy po�o�eniem rzeczywistym (symulowanym) a ekstrapolowanym
float sum_of_angle_differences = 0;             // sumaryczna r�nica k�towa -||- 


struct Frame                                      // g��wna struktura s�u��ca do przesy�ania informacji
{	
	int iID;                                      // identyfikator obiektu, kt�rego 
	int type;                                     // typ ramki: informacja o stateie, informacja o zamkni�ciu, komunikat tekstowy, ... 
	ObjectState state;                            // po�o�enie, pr�dko��: �rodka masy + k�towe, ...

	long sending_time;                            // tzw. znacznik czasu potrzebny np. do obliczenia op�nienia
	int iID_receiver;                             // nr ID odbiorcy wiadomo�ci, je�li skierowana jest tylko do niego
};


//******************************************
// Funkcja obs�ugi w�tku odbioru komunikat�w 
// UWAGA!  Odbierane s� te� komunikaty z w�asnej aplikacji by por�wna� obraz ekstrapolowany do rzeczywistego.
DWORD WINAPI ReceiveThreadFun(void *ptr)
{
	multicast_net *pmt_net = (multicast_net*)ptr;  // wska�nik do obiektu klasy multicast_net
	Frame frame;

	while (1)
	{
		int frame_size = pmt_net->reciv((char*)&frame, sizeof(Frame));   // oczekiwanie na nadej�cie ramki 
		ObjectState state = frame.state;

		//fprintf(f, "odebrano stan iID = %d, ID dla mojego obiektu = %d\n", frame.iID, my_vehicle->iID);

		// Lock the Critical section
		EnterCriticalSection(&m_cs);               // wej�cie na �cie�k� krytyczn� - by inne w�tki (np. g��wny) nie wsp�dzieli� 
	                                               // tablicy other_vehicles

		if ((if_shadow) || (frame.iID != my_vehicle->iID))          // je�li to nie m�j w�asny obiekt
		{
			
			if ((other_vehicles.size() == 0) || (other_vehicles[frame.iID] == NULL))        // nie ma jeszcze takiego obiektu w tablicy -> trzeba go
				// stworzy�
			{
				MovableObject *ob = new MovableObject();
				ob->iID = frame.iID;
				ob->time_of_last_update = clock();
				other_vehicles[frame.iID] = ob;		
				//fprintf(f, "zarejestrowano %d obcy obiekt o ID = %d\n", iLiczbaCudzychOb - 1, CudzeObiekty[iLiczbaCudzychOb]->iID);
			}
			other_vehicles[frame.iID]->ChangeState(state);   // aktualizacja stateu obiektu obcego 	
			
		}	
		//Release the Critical section
		LeaveCriticalSection(&m_cs);               // wyj�cie ze �cie�ki krytycznej
	}  // while(1)
	return 1;
}

// *****************************************************************
// ****    Wszystko co trzeba zrobi� podczas uruchamiania aplikacji
// ****    poza grafik�   
void InteractionInitialization()
{
	DWORD dwThreadId;

	my_vehicle = new MovableObject();    // tworzenie wlasnego obiektu

	time_of_cycle = clock();             // pomiar aktualnego czasu

	// obiekty sieciowe typu multicast (z podaniem adresu WZR oraz numeru portu)
	multi_reciv = new multicast_net("224.12.13.181", 10001);      // obiekt do odbioru ramek sieciowych
	multi_send = new multicast_net("224.12.13.181", 10001);       // obiekt do wysy�ania ramek


	// uruchomienie w�tku obs�uguj�cego odbi�r komunikat�w:
	threadReciv = CreateThread(
		NULL,                        // no security attributes
		0,                           // use default stack size
		ReceiveThreadFun,                // thread function
		(void *)multi_reciv,               // argument to thread function
		NULL,                        // use default creation flags
		&dwThreadId);                // returns the thread identifier
	SetThreadPriority(threadReciv, THREAD_PRIORITY_HIGHEST);



	fprintf(f,"poczatek interakcji\n");
}


// *****************************************************************
// ****    Wszystko co trzeba zrobi� w ka�dym cyklu dzia�ania 
// ****    aplikacji poza grafik� 
void VirtualWorldCycle()
{
	number_of_cyc++;
	float time_from_start_in_s = (float)(clock() - time_start) / CLOCKS_PER_SEC;  // czas w sek. jaki up�yn�� od uruchomienia programu

	if (number_of_cyc % 50 == 0)          // je�li licznik cykli przekroczy� pewn� warto��, to
	{                              // nale�y na nowo obliczy� �redni czas cyklu avg_cycle_time
		char text[256];
		long prev_time = time_of_cycle;
		time_of_cycle = clock();
		float fFps = (50 * CLOCKS_PER_SEC) / (float)(time_of_cycle - prev_time);
		if (fFps != 0) avg_cycle_time = 1.0 / fFps; else avg_cycle_time = 1;
	
		sprintf(text, "WZR 2020, tem.2, wersja c, cz�sto�� pr.wys. = %0.2f[r/s]  �r.odl = %0.3f[m]  �r.ro�n.k�t. = %0.3f[st]",
			(float)number_of_send_trials / time_from_start_in_s, sum_differences_of_pos / number_of_cyc, 
			sum_of_angle_differences / number_of_cyc*180.0 / 3.14159);

		if (time_from_start_in_s > 5)
			SetWindowText(window_handle, text); // wy�wietlenie aktualnych odchy�ek						
	}

	// obliczenie �redniej odleg�o�ci i �redniej r�nicy k�towej pomi�dzy pojazdem a cieniem:
	EnterCriticalSection(&m_cs);
	MovableObject *car = (other_vehicles.size() > 0 ? other_vehicles[my_vehicle->iID] : NULL);       
	if (car != NULL)
	{
		sum_differences_of_pos += DistanceBetweenPointsOnTetraMap(my_vehicle->state.vPos, car->state.vPos);
		sum_of_angle_differences += AngleBetweenQuats(my_vehicle->state.qOrient, car->state.qOrient);
	}
	else {
		sum_differences_of_pos += DistanceBetweenPointsOnTetraMap(my_vehicle->state.vPos, Vector3(0,0,0));  
		sum_of_angle_differences += AngleBetweenQuats(my_vehicle->state.qOrient, quaternion(0, 0, 0, 1));
	}
	LeaveCriticalSection(&m_cs);
	
	// test predykcji:
	if (if_prediction_test)
	{
		int number_of_actions = sizeof(test_scenario) / (4 * sizeof(float));
		bool test_finished = test_scenario_step(my_vehicle, test_scenario, number_of_actions, time_from_start_in_s);

		if (test_finished) // czas dobiegl konca -> koniec testu 
		{
			if_prediction_test = false;
			char text[200];
			sprintf(text, "Po czasie %3.2f[s]  �r.cz�sto�� = %0.2f[r/s]  �r.odl = %0.3f[m]  �r.r�n.k�t. = %0.3f[st]",
				time_from_start_in_s, (float)number_of_send_trials / time_from_start_in_s, 
				sum_differences_of_pos / number_of_cyc, sum_of_angle_differences / number_of_cyc*180.0 / 3.14159);
			fprintf(f, "%s\n", text);
			MessageBox(window_handle, text, "Test predykcji", MB_OK);
		}
	}

	my_vehicle->Simulation(avg_cycle_time);                    // symulacja w�asnego obiektu

	time_from_start_in_s = (float)(clock() - time_start) / CLOCKS_PER_SEC;
	
	//if ((float)(clock() - time_last_send) / CLOCKS_PER_SEC >= 0.2 + 30 * (time_from_start_in_s < 30))
	if (my_vehicle->F > 50 || my_vehicle->F < -50) {
		if ((float)(clock() - time_last_send) / CLOCKS_PER_SEC >= 1.0)
		{
			Frame frame;
			frame.state = my_vehicle->State();                   // stan w�asnego obiektu 
			frame.iID = my_vehicle->iID;
			multi_send->send((char*)&frame, sizeof(Frame));  // wys�anie komunikatu do pozosta�ych aplikacji co pewien czas
			time_last_send = clock();
			number_of_send_trials++;
		}
	}
	else if (my_vehicle->state.wheel_angle != 0) {
		if ((float)(clock() - time_last_send) / CLOCKS_PER_SEC >= 0.5)
		{
			Frame frame;
			frame.state = my_vehicle->State();                   // stan w�asnego obiektu 
			frame.iID = my_vehicle->iID;
			multi_send->send((char*)&frame, sizeof(Frame));  // wys�anie komunikatu do pozosta�ych aplikacji co pewien czas
			time_last_send = clock();
			number_of_send_trials++;
		}
	}
	else {
		if ((float)(clock() - time_last_send) / CLOCKS_PER_SEC >= 2.0)
		{
			Frame frame;
			frame.state = my_vehicle->State();                   // stan w�asnego obiektu 
			frame.iID = my_vehicle->iID;
			multi_send->send((char*)&frame, sizeof(Frame));  // wys�anie komunikatu do pozosta�ych aplikacji co pewien czas
			time_last_send = clock();
			number_of_send_trials++;
		}
	}
	

	//       ----------------------------------
	//    -------------------------------------
	// ----------------------------------------
	// ------------  Miejsce na predykcj� stanu:
	// Lock the Critical section
	EnterCriticalSection(&m_cs);
	for (map<int, MovableObject*>::iterator it = other_vehicles.begin(); it != other_vehicles.end(); ++it)
	{
		MovableObject *veh = it->second;

		if (veh)
		{
			float dt = (float)((clock() - veh->time_of_last_update)/ CLOCKS_PER_SEC);
			veh->state.vV = veh->state.vV + veh->state.vA * dt / 10;
			veh->state.vPos = veh->state.vPos + veh->state.vV * dt;
			Vector3 w_obrot = veh->state.vV_ang * dt + veh->state.vA_ang * dt * dt / 2;
			quaternion q_obrot = AsixToQuat(w_obrot.znorm(), w_obrot.length());
			veh->state.qOrient = q_obrot * veh->state.qOrient;
			//veh->state.vV_ang = ....
			veh->time_of_last_update = clock();
		}

	}
	//Release the Critical section
	LeaveCriticalSection(&m_cs);
}

// *****************************************************************
// ****    Wszystko co trzeba zrobi� podczas zamykania aplikacji
// ****    poza grafik� 
void EndOfInteraction()
{
	fprintf(f, "Koniec interakcji\n");
	fclose(f);
}

//deklaracja funkcji obslugi okna
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

HDC g_context = NULL;        // uchwyt contextu graficznego



//funkcja Main - dla Windows
int WINAPI WinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR     lpCmdLine,
	int       nCmdShow)
{
	
	//Initilize the critical section
	InitializeCriticalSection(&m_cs);

	MSG message;		  //innymi slowy "komunikat"
	WNDCLASS main_class; //klasa g��wnego okna aplikacji

	static char class_name[] = "Klasa_Podstawowa";

	//Definiujemy klase g��wnego okna aplikacji
	//Okreslamy tu wlasciwosci okna, szczegoly wygladu oraz
	//adres funkcji przetwarzajacej komunikaty
	main_class.style = CS_HREDRAW | CS_VREDRAW;
	main_class.lpfnWndProc = WndProc; //adres funkcji realizuj�cej przetwarzanie meldunk�w 
	main_class.cbClsExtra = 0;
	main_class.cbWndExtra = 0;
	main_class.hInstance = hInstance; //identyfikator procesu przekazany przez MS Windows podczas uruchamiania programu
	main_class.hIcon = 0;
	main_class.hCursor = LoadCursor(0, IDC_ARROW);
	main_class.hbrBackground = (HBRUSH)GetStockObject(GRAY_BRUSH);
	main_class.lpszMenuName = "Menu";
	main_class.lpszClassName = class_name;

	//teraz rejestracja klasy okna g��wnego
	RegisterClass(&main_class);

	window_handle = CreateWindow(class_name, "WZR 2020 temat 2 Naw.obl., wersja d", WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
		500, 50, 850, 650, NULL, NULL, hInstance, NULL);

	ShowWindow(window_handle, nCmdShow);

	//odswiezamy zawartosc okna
	UpdateWindow(window_handle);

	// pobranie komunikatu z kolejki je�li funkcja PeekMessage zwraca warto�� inn� ni� FALSE,
	// w przeciwnym wypadku symulacja wirtualnego �wiata wraz z wizualizacj�
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
			VirtualWorldCycle();    // Cykl wirtualnego �wiata
			InvalidateRect(window_handle, NULL, FALSE);
		}
	}

	return (int)message.wParam;
}

/********************************************************************
FUNKCJA OKNA realizujaca przetwarzanie meldunk�w kierowanych do okna aplikacji*/
LRESULT CALLBACK WndProc(HWND window_handle, UINT message_code, WPARAM wParam, LPARAM lParam)
{

	switch (message_code)
	{
	case WM_CREATE:  //message wysy�any w momencie tworzenia okna
	{

		g_context = GetDC(window_handle);

		srand((unsigned)time(NULL));
		int result = GraphicsInitialization(g_context);
		if (result == 0)
		{
			printf("nie udalo sie otworzyc okna graficznego\n");
			//exit(1);
		}

		InteractionInitialization();

		SetTimer(window_handle, 1, 10, NULL);

		time_start = clock();      // by czas liczy� po utworzeniu okna i inicjalizacji 

		return 0;
	}


	case WM_PAINT:
	{
		PAINTSTRUCT paint;
		HDC context;
		context = BeginPaint(window_handle, &paint);

		DrawGLScene();
		SwapBuffers(context);

		EndPaint(window_handle, &paint);

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

	case WM_DESTROY: //obowi�zkowa obs�uga meldunku o zamkni�ciu okna

		EndOfInteraction();
		EndOfGraphics();

		ReleaseDC(window_handle, g_context);
		KillTimer(window_handle, 1);

		//LPDWORD lpExitCode;
		DWORD ExitCode;
		GetExitCodeThread(threadReciv, &ExitCode);
		TerminateThread(threadReciv,ExitCode);
		//ExitThread(ExitCode);

		//Sleep(1000);

		other_vehicles.clear();
		

		PostQuitMessage(0);
		return 0;

	case WM_LBUTTONDOWN: //reakcja na lewy przycisk myszki
	{
		int x = LOWORD(lParam);
		int y = HIWORD(lParam);
		if (if_mouse_control)
			my_vehicle->F = 30.0;        // si�a pchaj�ca do przodu
		break;
	}
	case WM_RBUTTONDOWN: //reakcja na prawy przycisk myszki
	{
		int x = LOWORD(lParam);
		int y = HIWORD(lParam);
		if (if_mouse_control)
			my_vehicle->F = -5.0;        // si�a pchaj�ca do tylu
		break;
	}
	case WM_MBUTTONDOWN: //reakcja na �rodkowy przycisk myszki : uaktywnienie/dezaktywacja sterwania myszkowego
	{
		if_mouse_control = 1 - if_mouse_control;
		if (if_mouse_control) my_vehicle->if_keep_steer_wheel = true;
		else my_vehicle->if_keep_steer_wheel = false;

		mouse_cursor_x = LOWORD(lParam);
		mouse_cursor_y = HIWORD(lParam);
		break;
	}
	case WM_LBUTTONUP: //reakcja na puszczenie lewego przycisku myszki
	{
		if (if_mouse_control)
			my_vehicle->F = 0.0;        // si�a pchaj�ca do przodu
		break;
	}
	case WM_RBUTTONUP: //reakcja na puszczenie lewy przycisk myszki
	{
		if (if_mouse_control)
			my_vehicle->F = 0.0;        // si�a pchaj�ca do przodu
		break;
	}
	case WM_MOUSEMOVE:
	{
		int x = LOWORD(lParam);
		int y = HIWORD(lParam);
		if (if_mouse_control)
		{
			float wheel_angle = (float)(mouse_cursor_x - x) / 200;
			if (wheel_angle > my_vehicle->wheel_angle_max) wheel_angle = my_vehicle->wheel_angle_max;
			if (wheel_angle < -my_vehicle->wheel_angle_max) wheel_angle = -my_vehicle->wheel_angle_max;
			my_vehicle->state.wheel_angle = wheel_angle;
			//my_vehicle->wheel_turn_speed = (float)(mouse_cursor_x - x) / 20;
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
			my_vehicle->breaking_factor = 1.0;       // stopie� hamowania (reszta zale�y od si�y docisku i wsp. tarcia)
			break;                       // 1.0 to maksymalny stopie� (np. zablokowanie k�)
		}
		case VK_UP:
		{
			my_vehicle->F = 150.0;        // si�a pchaj�ca do przodu
			break;
		}
		case VK_DOWN:
		{
			my_vehicle->F = -110.0;
			break;
		}
		case VK_LEFT:
		{
			if (my_vehicle->wheel_turn_speed < 0){
				my_vehicle->wheel_turn_speed = 0;
				my_vehicle->if_keep_steer_wheel = true;
			}
			else{
				if (if_SHIFT_pressed) my_vehicle->wheel_turn_speed = 0.5;
				else my_vehicle->wheel_turn_speed = 0.25 / 8;
			}

			break;
		}
		case VK_RIGHT:
		{
			if (my_vehicle->wheel_turn_speed > 0){
				my_vehicle->wheel_turn_speed = 0;
				my_vehicle->if_keep_steer_wheel = true;
			}
			else{
				if (if_SHIFT_pressed) my_vehicle->wheel_turn_speed = -0.5;
				else my_vehicle->wheel_turn_speed = -0.25 / 8;
			}
			break;
		}
		case 'I':   // wypisywanie nr ID
		{
			if_ID_visible = 1 - if_ID_visible;
			break;
		}
		case 'W':   // cam_distance widoku
		{
			//cam_pos = cam_pos - cam_direct*0.3;
			if (view_parameters.cam_distance > 0.5) view_parameters.cam_distance /= 1.2;
			else view_parameters.cam_distance = 0;
			break;
		}
		case 'S':   // przybli�enie widoku
		{
			//cam_pos = cam_pos + cam_direct*0.3; 
			if (view_parameters.cam_distance > 0) view_parameters.cam_distance *= 1.2;
			else view_parameters.cam_distance = 0.5;
			break;
		}
		case 'Q':   // widok z g�ry
		{
			if (view_parameters.tracking) break;
			view_parameters.top_view = 1 - view_parameters.top_view;
			if (view_parameters.top_view)
			{
				view_parameters.cam_pos_1 = view_parameters.cam_pos; view_parameters.cam_direct_1 = view_parameters.cam_direct; view_parameters.cam_vertical_1 = view_parameters.cam_vertical;
				view_parameters.cam_distance_1 = view_parameters.cam_distance; view_parameters.cam_angle_1 = view_parameters.cam_angle;
				view_parameters.cam_pos = view_parameters.cam_pos_2; view_parameters.cam_direct = view_parameters.cam_direct_2; view_parameters.cam_vertical = view_parameters.cam_vertical_2;
				view_parameters.cam_distance = view_parameters.cam_distance_2; view_parameters.cam_angle = view_parameters.cam_angle_2;
			}
			else
			{
				view_parameters.cam_pos_2 = view_parameters.cam_pos; view_parameters.cam_direct_2 = view_parameters.cam_direct; view_parameters.cam_vertical_2 = view_parameters.cam_vertical;
				view_parameters.cam_distance_2 = view_parameters.cam_distance; view_parameters.cam_angle_2 = view_parameters.cam_angle;
				view_parameters.cam_pos = view_parameters.cam_pos_1; view_parameters.cam_direct = view_parameters.cam_direct_1; view_parameters.cam_vertical = view_parameters.cam_vertical_1;
				view_parameters.cam_distance = view_parameters.cam_distance_1; view_parameters.cam_angle = view_parameters.cam_angle_1;
			}
			break;
		}
		case 'E':   // obr�t kamery ku g�rze (wzgl�dem lokalnej osi z)
		{
			view_parameters.cam_angle += PI * 5 / 180;
			break;
		}
		case 'D':   // obr�t kamery ku do�owi (wzgl�dem lokalnej osi z)
		{
			view_parameters.cam_angle -= PI * 5 / 180;
			break;
		}
		case 'A':   // w��czanie, wy��czanie trybu �ledzenia obiektu
		{
			view_parameters.tracking = 1 - view_parameters.tracking;
			if (view_parameters.tracking)
			{
				view_parameters.cam_distance = view_parameters.cam_distance_3; view_parameters.cam_angle = view_parameters.cam_angle_3;
			}
			else
			{
				view_parameters.cam_distance_3 = view_parameters.cam_distance; view_parameters.cam_angle_3 = view_parameters.cam_angle;
				view_parameters.top_view = 0;
				view_parameters.cam_pos = view_parameters.cam_pos_1; view_parameters.cam_direct = view_parameters.cam_direct_1; view_parameters.cam_vertical = view_parameters.cam_vertical_1;
				view_parameters.cam_distance = view_parameters.cam_distance_1; view_parameters.cam_angle = view_parameters.cam_angle_1;
			}
			break;
		}
		case 'Z':   // zoom - zmniejszenie k�ta widzenia
		{
			view_parameters.zoom /= 1.1;
			RECT rc;
			GetClientRect(window_handle, &rc);
			WindowResize(rc.right - rc.left, rc.bottom - rc.top);
			break;
		}
		case 'X':   // zoom - zwi�kszenie k�ta widzenia
		{
			view_parameters.zoom *= 1.1;
			RECT rc;
			GetClientRect(window_handle, &rc);
			WindowResize(rc.right - rc.left, rc.bottom - rc.top);
			break;
		}
		case VK_F1:  // wywolanie systemu pomocy
		{
			char lan[1024], lan_bie[1024];
			//GetSystemDirectory(lan_sys,1024);
			GetCurrentDirectory(1024, lan_bie);
			strcpy(lan, "C:\\Program Files\\Internet Explorer\\iexplore ");
			strcat(lan, lan_bie);
			strcat(lan, "\\pomoc.htm");
			int wyni = WinExec(lan, SW_NORMAL);
			if (wyni < 32)  // proba uruchominia pomocy nie powiodla sie
			{
				strcpy(lan, "C:\\Program Files\\Mozilla Firefox\\firefox ");
				strcat(lan, lan_bie);
				strcat(lan, "\\pomoc.htm");
				wyni = WinExec(lan, SW_NORMAL);
				if (wyni < 32)
				{
					char lan_win[1024];
					GetWindowsDirectory(lan_win, 1024);
					strcat(lan_win, "\\notepad pomoc.txt ");
					wyni = WinExec(lan_win, SW_NORMAL);
				}
			}
			break;
		}
		case VK_ESCAPE:
		{
			SendMessage(window_handle, WM_DESTROY, 0, 0);
			break;
		}
		} // switch po klawiszach

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
			//my_vehicle->state.wheel_angle = 0;
			if (my_vehicle->if_keep_steer_wheel) my_vehicle->wheel_turn_speed = -0.25/8;
			else my_vehicle->wheel_turn_speed = 0; 
			my_vehicle->if_keep_steer_wheel = false;
			break;
		}
		case VK_RIGHT:
		{
			my_vehicle->Fb = 0.00;
			//my_vehicle->state.wheel_angle = 0;
			if (my_vehicle->if_keep_steer_wheel) my_vehicle->wheel_turn_speed = 0.25 / 8;
			else my_vehicle->wheel_turn_speed = 0;
			my_vehicle->if_keep_steer_wheel = false;
			break;
		}

		}

		break;
	}

	default: //statedardowa obs�uga pozosta�ych meldunk�w
		return DefWindowProc(window_handle, message_code, wParam, lParam);
	}


}

