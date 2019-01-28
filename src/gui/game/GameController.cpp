#include <iostream>
#include <queue>
#include "Config.h"
#include "Format.h"
#include "Platform.h"
#include "GameController.h"
#include "GameModel.h"
#include "gui/render/RenderController.h"
#include "gui/interface/Point.h"
#include "gui/dialogues/ErrorMessage.h"
#include "gui/dialogues/InformationMessage.h"
#include "gui/dialogues/ConfirmPrompt.h"
#include "GameModelException.h"
#include "simulation/Air.h"
#include "gui/elementsearch/ElementSearchActivity.h"
#include "gui/colourpicker/ColourPickerActivity.h"
#include "Notification.h"
#include "gui/interface/Keys.h"
#include "gui/interface/Mouse.h"
#include "simulation/Snapshot.h"
#include "debug/DebugInfo.h"
#include "debug/DebugParts.h"
#include "debug/ElementPopulation.h"
#include "debug/DebugLines.h"
#include "debug/ParticleDebug.h"
#ifdef LUACONSOLE
#include "lua/LuaScriptInterface.h"
#else
#include "lua/TPTScriptInterface.h"
#endif
#include "lua/LuaEvents.h"

using namespace std;

class GameController::SearchCallback: public ControllerCallback
{
	GameController * cc;
public:
	SearchCallback(GameController * cc_) { cc = cc_; }
	virtual void ControllerExit()
	{
	}
};

class GameController::SaveOpenCallback: public ControllerCallback
{
	GameController * cc;
public:
	SaveOpenCallback(GameController * cc_) { cc = cc_; }
	virtual void ControllerExit()
	{
	}
};

class GameController::OptionsCallback: public ControllerCallback
{
	GameController * cc;
public:
	OptionsCallback(GameController * cc_) { cc = cc_; }
	virtual void ControllerExit()
	{
		cc->gameModel->UpdateQuickOptions();
	}
};

class GameController::StampsCallback: public ControllerCallback
{
	GameController * cc;
public:
	StampsCallback(GameController * cc_) { cc = cc_; }
	virtual void ControllerExit()
	{
	}
};

GameController::GameController():
	firstTick(true),
	foundSignID(-1),
	renderOptions(NULL),
	console(NULL),
	options(NULL),
	debugFlags(0),
	HasDone(false)
{
	gameView = new GameView();
	gameModel = new GameModel();
	gameModel->BuildQuickOptionMenu(this);

	gameView->AttachController(this);
	gameModel->AddObserver(gameView);

#ifdef LUACONSOLE
	commandInterface = new LuaScriptInterface(this, gameModel);
	((LuaScriptInterface*)commandInterface)->SetWindow(gameView);
#else
	commandInterface = new TPTScriptInterface(this, gameModel);
#endif

	debugInfo.push_back(new DebugParts(0x1, gameModel->GetSimulation()));
	debugInfo.push_back(new ElementPopulationDebug(0x2, gameModel->GetSimulation()));
	debugInfo.push_back(new DebugLines(0x4, gameView, this));
	debugInfo.push_back(new ParticleDebug(0x8, gameModel->GetSimulation(), gameModel));
}

GameController::~GameController()
{
	if(renderOptions)
	{
		delete renderOptions;
	}
	if(console)
	{
		delete console;
	}
	if (options)
	{
		delete options;
	}
	for(std::vector<DebugInfo*>::iterator iter = debugInfo.begin(), end = debugInfo.end(); iter != end; iter++)
	{
		delete *iter;
	}
	//deleted here because it refuses to be deleted when deleted from gameModel even with the same code
	std::deque<Snapshot*> history = gameModel->GetHistory();
	for(std::deque<Snapshot*>::iterator iter = history.begin(), end = history.end(); iter != end; ++iter)
	{
		delete *iter;
	}
	std::vector<QuickOption*> quickOptions = gameModel->GetQuickOptions();
	for(std::vector<QuickOption*>::iterator iter = quickOptions.begin(), end = quickOptions.end(); iter != end; ++iter)
	{
		delete *iter;
	}
	std::vector<Notification*> notifications = gameModel->GetNotifications();
	for(std::vector<Notification*>::iterator iter = notifications.begin(); iter != notifications.end(); ++iter)
	{
		delete *iter;
	}
	delete gameModel;
	if (gameView->CloseActiveWindow())
	{
		delete gameView;
	}
}

void GameController::HistoryRestore()
{
}

void GameController::HistorySnapshot()
{
}

void GameController::HistoryForward()
{
}

GameView * GameController::GetView()
{
	return gameView;
}

int GameController::GetSignAt(int x, int y)
{
	Simulation * sim = gameModel->GetSimulation();
	for (int i = sim->signs.size()-1; i >= 0; i--)
	{
		int signx, signy, signw, signh;
		sim->signs[i].pos(sim->signs[i].getText(sim), signx, signy, signw, signh);
		if (x>=signx && x<=signx+signw && y>=signy && y<=signy+signh)
			return i;
	}
	return -1;
}

// assumed to already be a valid sign
String GameController::GetSignText(int signID)
{
	return gameModel->GetSimulation()->signs[signID].text;
}

void GameController::PlaceSave(ui::Point position, bool includePressure)
{
}

void GameController::Install()
{
}

void GameController::AdjustGridSize(int direction)
{
	if(direction > 0)
		gameModel->GetRenderer()->SetGridSize((gameModel->GetRenderer()->GetGridSize()+1)%10);
	else
		gameModel->GetRenderer()->SetGridSize((gameModel->GetRenderer()->GetGridSize()+9)%10);
}

void GameController::InvertAirSim()
{
	gameModel->GetSimulation()->air->Invert();
}


void GameController::AdjustBrushSize(int delta, bool logarithmic, bool xAxis, bool yAxis)
{
	if(xAxis && yAxis)
		return;

	ui::Point newSize(0, 0);
	ui::Point oldSize = gameModel->GetBrush()->GetRadius();
	if(logarithmic)
		newSize = gameModel->GetBrush()->GetRadius() + ui::Point(delta * std::max(gameModel->GetBrush()->GetRadius().X / 5, 1), delta * std::max(gameModel->GetBrush()->GetRadius().Y / 5, 1));
	else
		newSize = gameModel->GetBrush()->GetRadius() + ui::Point(delta, delta);
	if(newSize.X < 0)
		newSize.X = 0;
	if(newSize.Y < 0)
		newSize.Y = 0;
	if(newSize.X > 200)
		newSize.X = 200;
	if(newSize.Y > 200)
		newSize.Y = 200;

	if(xAxis)
		gameModel->GetBrush()->SetRadius(ui::Point(newSize.X, oldSize.Y));
	else if(yAxis)
		gameModel->GetBrush()->SetRadius(ui::Point(oldSize.X, newSize.Y));
	else
		gameModel->GetBrush()->SetRadius(newSize);
}

void GameController::SetBrushSize(ui::Point newSize)
{
	gameModel->GetBrush()->SetRadius(newSize);
}

void GameController::AdjustZoomSize(int delta, bool logarithmic)
{
	int newSize;
	if(logarithmic)
		newSize = gameModel->GetZoomSize() + std::max(gameModel->GetZoomSize() / 10, 1) * delta;
	else
		newSize = gameModel->GetZoomSize() + delta;
	if(newSize<5)
			newSize = 5;
	if(newSize>64)
			newSize = 64;
	gameModel->SetZoomSize(newSize);

	int newZoomFactor = 256/newSize;
	if(newZoomFactor<3)
		newZoomFactor = 3;
	gameModel->SetZoomFactor(newZoomFactor);
}

bool GameController::MouseInZoom(ui::Point position)
{
	if(position.X >= XRES)
		position.X = XRES-1;
	if(position.Y >= YRES)
		position.Y = YRES-1;
	if(position.Y < 0)
		position.Y = 0;
	if(position.X < 0)
		position.X = 0;

	return gameModel->MouseInZoom(position);
}

ui::Point GameController::PointTranslate(ui::Point point)
{
	if(point.X >= XRES)
		point.X = XRES-1;
	if(point.Y >= YRES)
		point.Y = YRES-1;
	if(point.Y < 0)
		point.Y = 0;
	if(point.X < 0)
		point.X = 0;

	return gameModel->AdjustZoomCoords(point);
}

ui::Point GameController::NormaliseBlockCoord(ui::Point point)
{
	return (point/CELL)*CELL;
}

void GameController::DrawRect(int toolSelection, ui::Point point1, ui::Point point2)
{
	Simulation * sim = gameModel->GetSimulation();
	Tool * activeTool = gameModel->GetActiveTool(toolSelection);
	gameModel->SetLastTool(activeTool);
	Brush * cBrush = gameModel->GetBrush();
	if(!activeTool || !cBrush)
		return;
	activeTool->SetStrength(1.0f);
	activeTool->DrawRect(sim, cBrush, point1, point2);
}

void GameController::DrawLine(int toolSelection, ui::Point point1, ui::Point point2)
{
	Simulation * sim = gameModel->GetSimulation();
	Tool * activeTool = gameModel->GetActiveTool(toolSelection);
	gameModel->SetLastTool(activeTool);
	Brush * cBrush = gameModel->GetBrush();
	if(!activeTool || !cBrush)
		return;
	activeTool->SetStrength(1.0f);
	activeTool->DrawLine(sim, cBrush, point1, point2);
}

void GameController::DrawFill(int toolSelection, ui::Point point)
{
	Simulation * sim = gameModel->GetSimulation();
	Tool * activeTool = gameModel->GetActiveTool(toolSelection);
	gameModel->SetLastTool(activeTool);
	Brush * cBrush = gameModel->GetBrush();
	if(!activeTool || !cBrush)
		return;
	activeTool->SetStrength(1.0f);
	activeTool->DrawFill(sim, cBrush, point);
}

void GameController::DrawPoints(int toolSelection, ui::Point oldPos, ui::Point newPos, bool held)
{
	Simulation * sim = gameModel->GetSimulation();
	Tool * activeTool = gameModel->GetActiveTool(toolSelection);
	gameModel->SetLastTool(activeTool);
	Brush * cBrush = gameModel->GetBrush();
	if (!activeTool || !cBrush)
	{
		return;
	}

	activeTool->SetStrength(gameModel->GetToolStrength());
	if (!held)
		activeTool->Draw(sim, cBrush, newPos);
	else
		activeTool->DrawLine(sim, cBrush, oldPos, newPos, true);
}

bool GameController::LoadClipboard()
{
}

void GameController::TranslateSave(ui::Point point)
{
}

void GameController::TransformSave(matrix2d transform)
{
}

void GameController::ToolClick(int toolSelection, ui::Point point)
{
	Simulation * sim = gameModel->GetSimulation();
	Tool * activeTool = gameModel->GetActiveTool(toolSelection);
	Brush * cBrush = gameModel->GetBrush();
	if(!activeTool || !cBrush)
		return;
	activeTool->Click(sim, cBrush, point);
}

ByteString GameController::StampRegion(ui::Point point1, ui::Point point2, bool includePressure)
{
}

void GameController::CopyRegion(ui::Point point1, ui::Point point2, bool includePressure)
{
}

void GameController::CutRegion(ui::Point point1, ui::Point point2, bool includePressure)
{
	CopyRegion(point1, point2, includePressure);
	gameModel->GetSimulation()->clear_area(point1.X, point1.Y, point2.X-point1.X, point2.Y-point1.Y);
}

bool GameController::MouseMove(int x, int y, int dx, int dy)
{
	MouseMoveEvent ev(x, y, dx, dy);
	return commandInterface->HandleEvent(LuaEvents::mousemove, &ev);
}

bool GameController::MouseDown(int x, int y, unsigned button)
{
	MouseDownEvent ev(x, y, button);
	bool ret = commandInterface->HandleEvent(LuaEvents::mousedown, &ev);
	if (ret && y<YRES && x<XRES && !gameView->GetPlacingSave() && !gameView->GetPlacingZoom())
	{
		ui::Point point = gameModel->AdjustZoomCoords(ui::Point(x, y));
		x = point.X;
		y = point.Y;
		if (!gameModel->GetActiveTool(0) || gameModel->GetActiveTool(0)->GetIdentifier() != "DEFAULT_UI_SIGN" || button != SDL_BUTTON_LEFT) //If it's not a sign tool or you are right/middle clicking
		{
			foundSignID = GetSignAt(x, y);
			if (foundSignID != -1)
			{
				sign foundSign = gameModel->GetSimulation()->signs[foundSignID];
				if (sign::splitsign(foundSign.text))
					return false;
			}
		}
	}
	return ret;
}

bool GameController::MouseUp(int x, int y, unsigned button, char type)
{
	MouseUpEvent ev(x, y, button, type);
	bool ret = commandInterface->HandleEvent(LuaEvents::mouseup, &ev);
	if (type)
		return ret;
	if (ret && foundSignID != -1 && y<YRES && x<XRES && !gameView->GetPlacingSave())
	{
		ui::Point point = gameModel->AdjustZoomCoords(ui::Point(x, y));
		x = point.X;
		y = point.Y;
		if (!gameModel->GetActiveTool(0) || gameModel->GetActiveTool(0)->GetIdentifier() != "DEFAULT_UI_SIGN" || button != SDL_BUTTON_LEFT) //If it's not a sign tool or you are right/middle clicking
		{
			int foundSignID = GetSignAt(x, y);
			if (foundSignID != -1)
			{
				sign foundSign = gameModel->GetSimulation()->signs[foundSignID];
				String str = foundSign.text;
				String::value_type type;
				int pos = sign::splitsign(str, &type);
				if (pos)
				{
					ret = false;
					if (type == 'c' || type == 't' || type == 's')
					{
						String link = str.Substr(3, pos-3);
						switch (type)
						{
						case 'c':
						{
							int saveID = link.ToNumber<int>(true);
							if (saveID)
								OpenSavePreview(saveID, 0, false);
							break;
						}
						case 't':
						{
							// buff is already confirmed to be a number by sign::splitsign
							Platform::OpenURI(ByteString::Build("http://powdertoy.co.uk/Discussions/Thread/View.html?Thread=", link.ToUtf8()));
							break;
						}
						case 's':
							OpenSearch(link);
							break;
						}
					}
					else if (type == 'b')
					{
						Simulation * sim = gameModel->GetSimulation();
						sim->create_part(-1, foundSign.x, foundSign.y, PT_SPRK);
					}
				}
			}
		}
	}
	foundSignID = -1;
	return ret;
}

bool GameController::MouseWheel(int x, int y, int d)
{
	MouseWheelEvent ev(x, y, d);
	return commandInterface->HandleEvent(LuaEvents::mousewheel, &ev);
}

bool GameController::TextInput(String text)
{
	TextInputEvent ev(text);
	return commandInterface->HandleEvent(LuaEvents::textinput, &ev);
}

bool GameController::KeyPress(int key, int scan, bool repeat, bool shift, bool ctrl, bool alt)
{
	KeyEvent ev(key, scan, repeat, shift, ctrl, alt);
	bool ret = commandInterface->HandleEvent(LuaEvents::keypress, &ev);
	if (repeat)
		return ret;
	if (ret)
	{
		Simulation * sim = gameModel->GetSimulation();
		if (!gameView->GetPlacingSave())
		{
			// Go right command
			if (key == SDLK_RIGHT)
			{
				sim->player.comm = (int)(sim->player.comm)|0x02;
			}
			// Go left command
			if (key == SDLK_LEFT)
			{
				sim->player.comm = (int)(sim->player.comm)|0x01;
			}
			// Use element command
			if (key == SDLK_DOWN && ((int)(sim->player.comm)&0x08)!=0x08)
			{
				sim->player.comm = (int)(sim->player.comm)|0x08;
			}
			// Jump command
			if (key == SDLK_UP && ((int)(sim->player.comm)&0x04)!=0x04)
			{
				sim->player.comm = (int)(sim->player.comm)|0x04;
			}
		}

		// Go right command
		if (key == SDLK_d)
		{
			sim->player2.comm = (int)(sim->player2.comm)|0x02;
		}
		// Go left command
		if (key == SDLK_a)
		{
			sim->player2.comm = (int)(sim->player2.comm)|0x01;
		}
		// Use element command
		if (key == SDLK_s && ((int)(sim->player2.comm)&0x08)!=0x08)
		{
			sim->player2.comm = (int)(sim->player2.comm)|0x08;
		}
		// Jump command
		if (key == SDLK_w && ((int)(sim->player2.comm)&0x04)!=0x04)
		{
			sim->player2.comm = (int)(sim->player2.comm)|0x04;
		}

		if (!sim->elementCount[PT_STKM2] || ctrl)
		{
			switch(key)
			{
			case 'w':
				SwitchGravity();
				break;
			case 'd':
				gameView->SetDebugHUD(!gameView->GetDebugHUD());
				break;
			case 's':
				gameView->BeginStampSelection();
				break;
			}
		}

		for(std::vector<DebugInfo*>::iterator iter = debugInfo.begin(), end = debugInfo.end(); iter != end; iter++)
		{
			if ((*iter)->debugID & debugFlags)
				if (!(*iter)->KeyPress(key, scan, shift, ctrl, alt, gameView->GetMousePosition()))
					ret = false;
		}
	}
	return ret;
}

bool GameController::KeyRelease(int key, int scan, bool repeat, bool shift, bool ctrl, bool alt)
{
	KeyEvent ev(key, scan, repeat, shift, ctrl, alt);
	bool ret = commandInterface->HandleEvent(LuaEvents::keyrelease, &ev);
	if (repeat)
		return ret;
	if (ret)
	{
		Simulation * sim = gameModel->GetSimulation();
		if (key == SDLK_RIGHT || key == SDLK_LEFT)
		{
			sim->player.pcomm = sim->player.comm;  //Saving last movement
			sim->player.comm = (int)(sim->player.comm)&12;  //Stop command
		}
		if (key == SDLK_UP)
		{
			sim->player.comm = (int)(sim->player.comm)&11;
		}
		if (key == SDLK_DOWN)
		{
			sim->player.comm = (int)(sim->player.comm)&7;
		}

		if (key == SDLK_d || key == SDLK_a)
		{
			sim->player2.pcomm = sim->player2.comm;  //Saving last movement
			sim->player2.comm = (int)(sim->player2.comm)&12;  //Stop command
		}
		if (key == SDLK_w)
		{
			sim->player2.comm = (int)(sim->player2.comm)&11;
		}
		if (key == SDLK_s)
		{
			sim->player2.comm = (int)(sim->player2.comm)&7;
		}
	}
	return ret;
}

void GameController::Tick()
{
	if(firstTick)
	{
#ifdef LUACONSOLE
		((LuaScriptInterface*)commandInterface)->Init();
#endif
		firstTick = false;
	}
	for(std::vector<DebugInfo*>::iterator iter = debugInfo.begin(), end = debugInfo.end(); iter != end; iter++)
	{
		if ((*iter)->debugID & debugFlags)
			(*iter)->Draw();
	}
	commandInterface->OnTick();
}

void GameController::Exit()
{
	CloseEvent ev;
	commandInterface->HandleEvent(LuaEvents::close, &ev);
	gameView->CloseActiveWindow();
	HasDone = true;
}

void GameController::ResetAir()
{
	Simulation * sim = gameModel->GetSimulation();
	sim->air->Clear();
	for (int i = 0; i < NPART; i++)
	{
		if (sim->parts[i].type == PT_QRTZ || sim->parts[i].type == PT_GLAS || sim->parts[i].type == PT_TUNG)
		{
			sim->parts[i].pavg[0] = sim->parts[i].pavg[1] = 0;
		}
	}
}

void GameController::ResetSpark()
{
	Simulation * sim = gameModel->GetSimulation();
	for (int i = 0; i < NPART; i++)
		if (sim->parts[i].type == PT_SPRK)
		{
			if (sim->parts[i].ctype >= 0 && sim->parts[i].ctype < PT_NUM && sim->elements[sim->parts[i].ctype].Enabled)
			{
				sim->parts[i].type = sim->parts[i].ctype;
				sim->parts[i].ctype = sim->parts[i].life = 0;
			}
			else
				sim->kill_part(i);
		}
	memset(sim->wireless, 0, sizeof(sim->wireless));
}

void GameController::SwitchGravity()
{
	gameModel->GetSimulation()->gravityMode = (gameModel->GetSimulation()->gravityMode+1)%3;

	switch (gameModel->GetSimulation()->gravityMode)
	{
	case 0:
		gameModel->SetInfoTip("Gravity: Vertical");
		break;
	case 1:
		gameModel->SetInfoTip("Gravity: Off");
		break;
	case 2:
		gameModel->SetInfoTip("Gravity: Radial");
		break;
	}
}

void GameController::SwitchAir()
{
	gameModel->GetSimulation()->air->airMode = (gameModel->GetSimulation()->air->airMode+1)%5;

	switch (gameModel->GetSimulation()->air->airMode)
	{
	case 0:
		gameModel->SetInfoTip("Air: On");
		break;
	case 1:
		gameModel->SetInfoTip("Air: Pressure Off");
		break;
	case 2:
		gameModel->SetInfoTip("Air: Velocity Off");
		break;
	case 3:
		gameModel->SetInfoTip("Air: Off");
		break;
	case 4:
		gameModel->SetInfoTip("Air: No Update");
		break;
	}
}

void GameController::ToggleAHeat()
{
	gameModel->SetAHeatEnable(!gameModel->GetAHeatEnable());
}

bool GameController::GetAHeatEnable()
{
	return gameModel->GetAHeatEnable();
}

void GameController::ToggleNewtonianGravity()
{
	gameModel->SetNewtonianGravity(!gameModel->GetNewtonianGrvity());
}

void GameController::LoadRenderPreset(int presetNum)
{
	Renderer * renderer = gameModel->GetRenderer();
	RenderPreset preset = renderer->renderModePresets[presetNum];
	gameModel->SetInfoTip(preset.Name);
	renderer->SetRenderMode(preset.RenderModes);
	renderer->SetDisplayMode(preset.DisplayModes);
	renderer->SetColourMode(preset.ColourMode);
}

void GameController::Update()
{
	ui::Point pos = gameView->GetMousePosition();
	gameModel->GetRenderer()->mousePos = PointTranslate(pos);
	if (pos.X < XRES && pos.Y < YRES)
		gameView->SetSample(gameModel->GetSimulation()->GetSample(PointTranslate(pos).X, PointTranslate(pos).Y));
	else
		gameView->SetSample(gameModel->GetSimulation()->GetSample(pos.X, pos.Y));

	Simulation * sim = gameModel->GetSimulation();
	sim->BeforeSim();
	if (!sim->sys_pause || sim->framerender)
	{
		sim->UpdateParticles(0, NPART);
		sim->AfterSim();
	}

	//if either STKM or STK2 isn't out, reset it's selected element. Defaults to PT_DUST unless right selected is something else
	//This won't run if the stickmen dies in a frame, since it respawns instantly
	if (!sim->player.spwn || !sim->player2.spwn)
	{
		int rightSelected = PT_DUST;
		Tool * activeTool = gameModel->GetActiveTool(1);
		if (activeTool->GetIdentifier().BeginsWith("DEFAULT_PT_"))
		{
			int sr = activeTool->GetToolID();
			if (sr && sim->IsValidElement(sr))
				rightSelected = sr;
		}

		if (!sim->player.spwn)
			Element_STKM::STKM_set_element(sim, &sim->player, rightSelected);
		if (!sim->player2.spwn)
			Element_STKM::STKM_set_element(sim, &sim->player2, rightSelected);
	}
	if(renderOptions && renderOptions->HasExited)
	{
		delete renderOptions;
		renderOptions = NULL;
	}
}

void GameController::SetZoomEnabled(bool zoomEnabled)
{
	gameModel->SetZoomEnabled(zoomEnabled);
}

void GameController::SetToolStrength(float value)
{
	gameModel->SetToolStrength(value);
}

void GameController::SetZoomPosition(ui::Point position)
{
	ui::Point zoomPosition = position-(gameModel->GetZoomSize()/2);
	if(zoomPosition.X < 0)
			zoomPosition.X = 0;
	if(zoomPosition.Y < 0)
			zoomPosition.Y = 0;
	if(zoomPosition.X >= XRES-gameModel->GetZoomSize())
			zoomPosition.X = XRES-gameModel->GetZoomSize();
	if(zoomPosition.Y >= YRES-gameModel->GetZoomSize())
			zoomPosition.Y = YRES-gameModel->GetZoomSize();

	ui::Point zoomWindowPosition = ui::Point(0, 0);
	if(position.X < XRES/2)
		zoomWindowPosition.X = XRES-(gameModel->GetZoomSize()*gameModel->GetZoomFactor());

	gameModel->SetZoomPosition(zoomPosition);
	gameModel->SetZoomWindowPosition(zoomWindowPosition);
}

void GameController::SetPaused(bool pauseState)
{
	gameModel->SetPaused(pauseState);
}

void GameController::SetPaused()
{
	gameModel->SetPaused(!gameModel->GetPaused());
}

void GameController::SetDecoration(bool decorationState)
{
	gameModel->SetDecoration(decorationState);
}

void GameController::SetDecoration()
{
	gameModel->SetDecoration(!gameModel->GetDecoration());
}

void GameController::ShowGravityGrid()
{
	gameModel->ShowGravityGrid(!gameModel->GetGravityGrid());
	gameModel->UpdateQuickOptions();
}

void GameController::SetHudEnable(bool hudState)
{
	gameView->SetHudEnable(hudState);
}

bool GameController::GetHudEnable()
{
	return gameView->GetHudEnable();
}

void GameController::SetDebugHUD(bool hudState)
{
	gameView->SetDebugHUD(hudState);
}

bool GameController::GetDebugHUD()
{
	return gameView->GetDebugHUD();
}

void GameController::SetActiveColourPreset(int preset)
{
	gameModel->SetActiveColourPreset(preset);
}

void GameController::SetColour(ui::Colour colour)
{
	gameModel->SetColourSelectorColour(colour);
	gameModel->SetPresetColour(colour);
}

void GameController::SetActiveMenu(int menuID)
{
	gameModel->SetActiveMenu(menuID);
	if(menuID == SC_DECO)
		gameModel->SetColourSelectorVisibility(true);
	else
		gameModel->SetColourSelectorVisibility(false);
}

std::vector<Menu*> GameController::GetMenuList()
{
	return gameModel->GetMenuList();
}

int GameController::GetNumMenus(bool onlyEnabled)
{
	int count = 0;
	if (onlyEnabled)
	{
		std::vector<Menu*> menuList = gameModel->GetMenuList();
		for (std::vector<Menu*>::iterator it = menuList.begin(), end = menuList.end(); it != end; ++it)
			if ((*it)->GetVisible())
				count++;
	}
	else
		count = gameModel->GetMenuList().size();
	return count;
}

Tool * GameController::GetActiveTool(int selection)
{
	return gameModel->GetActiveTool(selection);
}

void GameController::SetActiveTool(int toolSelection, Tool * tool)
{
	if (gameModel->GetActiveMenu() == SC_DECO && toolSelection == 2)
		toolSelection = 0;
	gameModel->SetActiveTool(toolSelection, tool);
	gameModel->GetRenderer()->gravityZonesEnabled = false;
	if (toolSelection == 3)
		gameModel->GetSimulation()->replaceModeSelected = tool->GetToolID();
	gameModel->SetLastTool(tool);
	for(int i = 0; i < 3; i++)
	{
		if(gameModel->GetActiveTool(i) == gameModel->GetMenuList().at(SC_WALL)->GetToolList().at(WL_GRAV))
			gameModel->GetRenderer()->gravityZonesEnabled = true;
	}
	if(tool->GetIdentifier() == "DEFAULT_UI_PROPERTY")
		((PropertyTool *)tool)->OpenWindow(gameModel->GetSimulation());
}

void GameController::SetActiveTool(int toolSelection, ByteString identifier)
{
	Tool *tool = gameModel->GetToolFromIdentifier(identifier);
	if (!tool)
		return;
	SetActiveTool(toolSelection, tool);
}

void GameController::SetLastTool(Tool * tool)
{
	gameModel->SetLastTool(tool);
}

int GameController::GetReplaceModeFlags()
{
	return gameModel->GetSimulation()->replaceModeFlags;
}

void GameController::SetReplaceModeFlags(int flags)
{
	gameModel->GetSimulation()->replaceModeFlags = flags;
}

void GameController::OpenSearch(String searchText)
{
}

void GameController::OpenLocalSaveWindow(bool asCurrent)
{
}

void GameController::OpenSavePreview(int saveID, int saveDate, bool instant)
{
}

void GameController::OpenSavePreview()
{
}

void GameController::OpenLocalBrowse()
{
}

void GameController::OpenProfile()
{
}

void GameController::OpenElementSearch()
{
	vector<Tool*> toolList;
	vector<Menu*> menuList = gameModel->GetMenuList();
	for(std::vector<Menu*>::iterator iter = menuList.begin(), end = menuList.end(); iter!=end; ++iter) {
		if(!(*iter))
			continue;
		vector<Tool*> menuToolList = (*iter)->GetToolList();
		if(!menuToolList.size())
			continue;
		toolList.insert(toolList.end(), menuToolList.begin(), menuToolList.end());
	}
	vector<Tool*> hiddenTools = gameModel->GetUnlistedTools();
	toolList.insert(toolList.end(), hiddenTools.begin(), hiddenTools.end());
	new ElementSearchActivity(this, toolList);
}

void GameController::OpenColourPicker()
{
	class ColourPickerCallback: public ColourPickedCallback
	{
		GameController * c;
	public:
		ColourPickerCallback(GameController * _c): c(_c) {}
		virtual  ~ColourPickerCallback() {};
		virtual void ColourPicked(ui::Colour colour)
		{
			c->SetColour(colour);
		}
	};
	new ColourPickerActivity(gameModel->GetColourSelectorColour(), new ColourPickerCallback(this));
}

void GameController::OpenStamps()
{
}

void GameController::OpenOptions()
{
	options = new OptionsController(gameModel, new OptionsCallback(this));
	ui::Engine::Ref().ShowWindow(options->GetView());

}

void GameController::ShowConsole()
{
	if (!console)
		console = new ConsoleController(NULL, commandInterface);
	if (console->GetView() != ui::Engine::Ref().GetWindow())
		ui::Engine::Ref().ShowWindow(console->GetView());
}

void GameController::HideConsole()
{
	if (!console)
		return;
	console->GetView()->CloseActiveWindow();
}

void GameController::OpenRenderOptions()
{
	renderOptions = new RenderController(gameModel->GetRenderer(), NULL);
	ui::Engine::Ref().ShowWindow(renderOptions->GetView());
}

void GameController::OpenSaveWindow()
{
}

void GameController::SaveAsCurrent()
{
}

void GameController::FrameStep()
{
	gameModel->FrameStep(1);
	gameModel->SetPaused(true);
}

void GameController::Vote(int direction)
{
}

void GameController::ChangeBrush()
{
	gameModel->SetBrushID(gameModel->GetBrushID()+1);
}

void GameController::ClearSim()
{
	HistorySnapshot();
	gameModel->ClearSimulation();
}

void GameController::ReloadSim()
{
}

ByteString GameController::ElementResolve(int type, int ctype)
{
	if(gameModel && gameModel->GetSimulation())
	{
		if (type == PT_LIFE && ctype >= 0 && ctype < NGOL)
			return gameModel->GetSimulation()->gmenu[ctype].name;
		else if (type >= 0 && type < PT_NUM)
			return gameModel->GetSimulation()->elements[type].Name;
	}
	return "";
}

bool GameController::IsValidElement(int type)
{
	if(gameModel && gameModel->GetSimulation())
	{
		return (type && gameModel->GetSimulation()->IsValidElement(type));
	}
	else
		return false;
}

String GameController::WallName(int type)
{
	if(gameModel && gameModel->GetSimulation() && type >= 0 && type < UI_WALLCOUNT)
		return gameModel->GetSimulation()->wtypes[type].name;
	else
		return String();
}

int GameController::Record(bool record)
{
	return gameView->Record(record);
}

void GameController::RemoveNotification(Notification * notification)
{
	gameModel->RemoveNotification(notification);
}

void GameController::RunUpdater()
{
}
