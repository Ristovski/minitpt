#pragma once

#include <stack>
#include "common/Singleton.h"
#include "graphics/Graphics.h"
#include "Window.h"
#include "PowderToy.h"

namespace ui
{
	class Window;

	/* class Engine
	 *
	 * Controls the User Interface.
	 * Send user inputs to the Engine and the appropriate controls and components will interact.
	 */
	class Engine: public Singleton<Engine>
	{
	public:
		Engine();
		~Engine();

		void ShowWindow(Window * window);
		int CloseWindow();

		void onMouseMove(int x, int y);
		void onMouseClick(int x, int y, unsigned button);
		void onMouseUnclick(int x, int y, unsigned button);
		void onMouseWheel(int x, int y, int delta);
		void onKeyPress(int key, int scan, bool repeat, bool shift, bool ctrl, bool alt);
		void onKeyRelease(int key, int scan, bool repeat, bool shift, bool ctrl, bool alt);
		void onTextInput(String text);
		void onResize(int newWidth, int newHeight);
		void onClose();

		void Begin(int width, int height);
		inline bool Running();
		inline bool Broken();
		inline long unsigned int LastTick(); 
		inline void LastTick(long unsigned int tick); 
		void Exit();
		void ConfirmExit();
		void Break();
		void UnBreak();

		void SetFullscreen(bool fullscreen); 
		inline bool GetFullscreen(); 
		void SetAltFullscreen(bool altFullscreen); 
		inline bool GetAltFullscreen(); 
		void SetScale(int scale); 
		inline int GetScale(); 
		void SetResizable(bool resizable); 
		inline bool GetResizable(); 
		void SetFastQuit(bool fastquit); 
		inline bool GetFastQuit(); 

		void Tick();
		void Draw();

		void SetFps(float fps);
		inline float GetFps(); 

		inline int GetMouseButton(); 
		inline int GetMouseX(); 
		inline int GetMouseY(); 
		inline int GetWidth(); 
		inline int GetHeight(); 
		inline int GetMaxWidth(); 
		inline int GetMaxHeight(); 

		void SetMaxSize(int width, int height);

		inline void SetSize(int width, int height);

		//void SetState(Window* state);
		//inline State* GetState() { return state_; }
		inline Window* GetWindow(); 
		float FpsLimit;
		Graphics * g;
		int Scale;
		bool Fullscreen;

		unsigned int FrameIndex;
	private:
		bool altFullscreen;
		bool resizable;

		float dt;
		float fps;
		pixel * lastBuffer;
		std::stack<pixel*> prevBuffers;
		std::stack<Window*> windows;
		std::stack<Point> mousePositions;
		//Window* statequeued_;
		Window* state_;
		Point windowTargetPosition;
		int windowOpenState;
		bool ignoreEvents = false;

		bool running_;
		bool break_;
		bool FastQuit;

		long unsigned int lastTick;
		int mouseb_;
		int mousex_;
		int mousey_;
		int mousexp_;
		int mouseyp_;
		int width_;
		int height_;

		int maxWidth;
		int maxHeight;
	};

}
