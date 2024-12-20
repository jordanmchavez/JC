#pragma once

#include "JC/Common.h"

namespace JC::Event { struct Event; }

namespace JC {

//--------------------------------------------------------------------------------------------------

struct App {
	virtual Res<> Init(Arena* perm, Arena* temp) = 0;
	virtual void  Shutdown() = 0;
	virtual Res<> Events(Span<Event::Event> events) = 0;
	virtual Res<> Update(double secs) = 0;
	virtual Res<> Draw() = 0;

	void Exit();
/*
Config Files	Game configuration settings.		
love.draw	Callback function used to draw on the screen every frame.		
love.errorhandler	The error handler, used to display error messages.	Added since 11.0	
love.load	This function is called exactly once at the beginning of the game.		
love.quit	Callback function triggered when the game is closed.	Added since 0.7.0	
love.run	The main function, containing the main loop. A sensible default is used when left out.		
love.threaderror	Callback function triggered when a Thread encounters an error.	Added since 0.9.0	
love.update	Callback function used to update the state of the game every frame.		

Window
love.focus	Callback function triggered when window receives or loses focus.	Added since 0.7.0	
love.resize	Called when the window is resized.	Added since 0.9.0	
love.visible	Callback function triggered when window is shown or hidden.	Added since 0.9.0	

Keyboard
love.keypressed	Callback function triggered when a key is pressed.		
love.keyreleased	Callback function triggered when a keyboard key is released.		
love.textedited	Called when the candidate text for an IME has changed.	Added since 0.10.0	
love.textinput	Called when text has been entered by the user.	Added since 0.9.0	

Mouse
love.mousemoved	Callback function triggered when the mouse is moved.	Added since 0.9.2	
love.mousepressed	Callback function triggered when a mouse button is pressed.		
love.mousereleased	Callback function triggered when a mouse button is released.		
love.wheelmoved	Callback function triggered when the mouse wheel is moved.	Added since 0.10.0	
*/
};

void RunApp(App* app);

//--------------------------------------------------------------------------------------------------

}	// namespace JC
