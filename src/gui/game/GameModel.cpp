#include "GameModel.h"
#include "GameView.h"
#include "ToolClasses.h"
#include "Brush.h"
#include "EllipseBrush.h"
#include "TriangleBrush.h"
#include "BitmapBrush.h"
#include "QuickOptions.h"
#include "GameModelException.h"
#include "Format.h"

#include "common/tpt-minmax.h"
#include "graphics/Renderer.h"
#include "simulation/Air.h"
#include "simulation/Simulation.h"
#include "simulation/Snapshot.h"

#include "gui/game/DecorationTool.h"
#include "gui/interface/Engine.h"
#include "gui/interface/Point.h"


GameModel::GameModel():
	activeMenu(-1),
	currentBrush(0),
	currentFile(NULL),
	toolStrength(1.0f),
	redoHistory(NULL),
	historyPosition(0),
	activeColourPreset(0),
	colourSelector(false),
	colour(255, 0, 0, 255),
	edgeMode(0)
{
	sim = new Simulation();
	ren = new Renderer(ui::Engine::Ref().g, sim);

	activeTools = regularToolset;

	std::fill(decoToolset, decoToolset+4, (Tool*)NULL);
	std::fill(regularToolset, regularToolset+4, (Tool*)NULL);

	//Default render prefs
	std::vector<unsigned int> tempArray;
	tempArray.push_back(RENDER_FIRE);
	tempArray.push_back(RENDER_EFFE);
	tempArray.push_back(RENDER_BASC);
	ren->SetRenderMode(tempArray);
	tempArray.clear();

	ren->SetDisplayMode(tempArray);

	ren->SetColourMode(0);

	BuildMenus();

	//Set default brush palette
	brushList.push_back(new EllipseBrush(ui::Point(4, 4)));
	brushList.push_back(new Brush(ui::Point(4, 4)));
	brushList.push_back(new TriangleBrush(ui::Point(4, 4)));

	colourPresets.push_back(ui::Colour(255, 255, 255));
	colourPresets.push_back(ui::Colour(0, 255, 255));
	colourPresets.push_back(ui::Colour(255, 0, 255));
	colourPresets.push_back(ui::Colour(255, 255, 0));
	colourPresets.push_back(ui::Colour(255, 0, 0));
	colourPresets.push_back(ui::Colour(0, 255, 0));
	colourPresets.push_back(ui::Colour(0, 0, 255));
	colourPresets.push_back(ui::Colour(0, 0, 0));

	undoHistoryLimit = 200;
}

GameModel::~GameModel()
{
	for (size_t i = 0; i < menuList.size(); i++)
	{
		if (i == SC_FAVORITES)
			menuList[i]->ClearTools();
		delete menuList[i];
	}
	for (std::vector<Tool*>::iterator iter = extraElementTools.begin(), end = extraElementTools.end(); iter != end; ++iter)
	{
		delete *iter;
	}
	for (size_t i = 0; i < brushList.size(); i++)
	{
		delete brushList[i];
	}
	delete sim;
	delete ren;
	delete currentFile;
	delete redoHistory;
	//if(activeTools)
	//	delete[] activeTools;
}

void GameModel::UpdateQuickOptions()
{
	for(std::vector<QuickOption*>::iterator iter = quickOptions.begin(), end = quickOptions.end(); iter != end; ++iter)
	{
		QuickOption * option = *iter;
		option->Update();
	}
}

void GameModel::BuildQuickOptionMenu(GameController * controller)
{
	for(std::vector<QuickOption*>::iterator iter = quickOptions.begin(), end = quickOptions.end(); iter != end; ++iter)
	{
		delete *iter;
	}
	quickOptions.clear();

	quickOptions.push_back(new SandEffectOption(this));
	quickOptions.push_back(new DrawGravOption(this));
	quickOptions.push_back(new DecorationsOption(this));
	quickOptions.push_back(new NGravityOption(this));
	quickOptions.push_back(new AHeatOption(this));
	quickOptions.push_back(new ConsoleShowOption(this, controller));

	notifyQuickOptionsChanged();
	UpdateQuickOptions();
}

void GameModel::BuildMenus()
{
	int lastMenu = -1;
	if(activeMenu != -1)
		lastMenu = activeMenu;

	ByteString activeToolIdentifiers[4];
	if(regularToolset[0])
		activeToolIdentifiers[0] = regularToolset[0]->GetIdentifier();
	if(regularToolset[1])
		activeToolIdentifiers[1] = regularToolset[1]->GetIdentifier();
	if(regularToolset[2])
		activeToolIdentifiers[2] = regularToolset[2]->GetIdentifier();
	if(regularToolset[3])
		activeToolIdentifiers[3] = regularToolset[3]->GetIdentifier();

	//Empty current menus
	for (size_t i = 0; i < menuList.size(); i++)
	{
		if (i == SC_FAVORITES)
			menuList[i]->ClearTools();
		delete menuList[i];
	}
	menuList.clear();
	toolList.clear();

	for(std::vector<Tool*>::iterator iter = extraElementTools.begin(), end = extraElementTools.end(); iter != end; ++iter)
	{
		delete *iter;
	}
	extraElementTools.clear();
	elementTools.clear();

	//Create menus
	for (int i = 0; i < SC_TOTAL; i++)
	{
		menuList.push_back(new Menu(sim->msections[i].icon, sim->msections[i].name, sim->msections[i].doshow));
	}

	//Build menus from Simulation elements
	for(int i = 0; i < PT_NUM; i++)
	{
		if(sim->elements[i].Enabled)
		{
			Tool * tempTool;
			if(i == PT_LIGH)
			{
				tempTool = new Element_LIGH_Tool(i, sim->elements[i].Name, sim->elements[i].Description, PIXR(sim->elements[i].Colour), PIXG(sim->elements[i].Colour), PIXB(sim->elements[i].Colour), sim->elements[i].Identifier, sim->elements[i].IconGenerator);
			}
			else if(i == PT_TESC)
			{
				tempTool = new Element_TESC_Tool(i, sim->elements[i].Name, sim->elements[i].Description, PIXR(sim->elements[i].Colour), PIXG(sim->elements[i].Colour), PIXB(sim->elements[i].Colour), sim->elements[i].Identifier, sim->elements[i].IconGenerator);
			}
			else if(i == PT_STKM || i == PT_FIGH || i == PT_STKM2)
			{
				tempTool = new PlopTool(i, sim->elements[i].Name, sim->elements[i].Description, PIXR(sim->elements[i].Colour), PIXG(sim->elements[i].Colour), PIXB(sim->elements[i].Colour), sim->elements[i].Identifier, sim->elements[i].IconGenerator);
			}
			else
			{
				tempTool = new ElementTool(i, sim->elements[i].Name, sim->elements[i].Description, PIXR(sim->elements[i].Colour), PIXG(sim->elements[i].Colour), PIXB(sim->elements[i].Colour), sim->elements[i].Identifier, sim->elements[i].IconGenerator);
			}

			if (sim->elements[i].MenuSection >= 0 && sim->elements[i].MenuSection < SC_TOTAL && sim->elements[i].MenuVisible)
			{
				menuList[sim->elements[i].MenuSection]->AddTool(tempTool);
			}
			else
			{
				extraElementTools.push_back(tempTool);
			}
			elementTools.push_back(tempTool);
		}
	}

	//Build menu for GOL types
	for(int i = 0; i < NGOL; i++)
	{
		Tool * tempTool = new ElementTool(PT_LIFE|PMAPID(i), sim->gmenu[i].name, sim->gmenu[i].description, PIXR(sim->gmenu[i].colour), PIXG(sim->gmenu[i].colour), PIXB(sim->gmenu[i].colour), "DEFAULT_PT_LIFE_"+sim->gmenu[i].name);
		menuList[SC_LIFE]->AddTool(tempTool);
	}

	//Build other menus from wall data
	for(int i = 0; i < UI_WALLCOUNT; i++)
	{
		Tool * tempTool = new WallTool(i, "", sim->wtypes[i].descs, PIXR(sim->wtypes[i].colour), PIXG(sim->wtypes[i].colour), PIXB(sim->wtypes[i].colour), sim->wtypes[i].identifier, sim->wtypes[i].textureGen);
		menuList[SC_WALL]->AddTool(tempTool);
		//sim->wtypes[i]
	}

	//Build menu for tools
	for (size_t i = 0; i < sim->tools.size(); i++)
	{
		Tool * tempTool;
		tempTool = new Tool(i, sim->tools[i]->Name, sim->tools[i]->Description, PIXR(sim->tools[i]->Colour), PIXG(sim->tools[i]->Colour), PIXB(sim->tools[i]->Colour), sim->tools[i]->Identifier);
		menuList[SC_TOOL]->AddTool(tempTool);
	}
	//Add special sign and prop tools
	menuList[SC_TOOL]->AddTool(new WindTool(0, "WIND", "Creates air movement.", 64, 64, 64, "DEFAULT_UI_WIND"));
	menuList[SC_TOOL]->AddTool(new PropertyTool());
	menuList[SC_TOOL]->AddTool(new SignTool(this));
	menuList[SC_TOOL]->AddTool(new SampleTool(this));

	//Add decoration tools to menu
	menuList[SC_DECO]->AddTool(new DecorationTool(ren, DECO_ADD, "ADD", "Colour blending: Add.", 0, 0, 0, "DEFAULT_DECOR_ADD"));
	menuList[SC_DECO]->AddTool(new DecorationTool(ren, DECO_SUBTRACT, "SUB", "Colour blending: Subtract.", 0, 0, 0, "DEFAULT_DECOR_SUB"));
	menuList[SC_DECO]->AddTool(new DecorationTool(ren, DECO_MULTIPLY, "MUL", "Colour blending: Multiply.", 0, 0, 0, "DEFAULT_DECOR_MUL"));
	menuList[SC_DECO]->AddTool(new DecorationTool(ren, DECO_DIVIDE, "DIV", "Colour blending: Divide." , 0, 0, 0, "DEFAULT_DECOR_DIV"));
	menuList[SC_DECO]->AddTool(new DecorationTool(ren, DECO_SMUDGE, "SMDG", "Smudge tool, blends surrounding deco together.", 0, 0, 0, "DEFAULT_DECOR_SMDG"));
	menuList[SC_DECO]->AddTool(new DecorationTool(ren, DECO_CLEAR, "CLR", "Erase any set decoration.", 0, 0, 0, "DEFAULT_DECOR_CLR"));
	menuList[SC_DECO]->AddTool(new DecorationTool(ren, DECO_DRAW, "SET", "Draw decoration (No blending).", 0, 0, 0, "DEFAULT_DECOR_SET"));
	SetColourSelectorColour(colour); // update tool colors
	decoToolset[0] = GetToolFromIdentifier("DEFAULT_DECOR_SET");
	decoToolset[1] = GetToolFromIdentifier("DEFAULT_DECOR_CLR");
	decoToolset[2] = GetToolFromIdentifier("DEFAULT_UI_SAMPLE");
	decoToolset[3] = GetToolFromIdentifier("DEFAULT_PT_NONE");

	//Set default tools
	regularToolset[0] = GetToolFromIdentifier("DEFAULT_PT_DUST");
	regularToolset[1] = GetToolFromIdentifier("DEFAULT_PT_NONE");
	regularToolset[2] = GetToolFromIdentifier("DEFAULT_UI_SAMPLE");
	regularToolset[3] = GetToolFromIdentifier("DEFAULT_PT_NONE");


	if(activeToolIdentifiers[0].length())
		regularToolset[0] = GetToolFromIdentifier(activeToolIdentifiers[0]);
	if(activeToolIdentifiers[1].length())
		regularToolset[1] = GetToolFromIdentifier(activeToolIdentifiers[1]);
	if(activeToolIdentifiers[2].length())
		regularToolset[2] = GetToolFromIdentifier(activeToolIdentifiers[2]);
	if(activeToolIdentifiers[3].length())
		regularToolset[3] = GetToolFromIdentifier(activeToolIdentifiers[3]);

	lastTool = activeTools[0];

	//Set default menu
	activeMenu = SC_POWDERS;

	if(lastMenu != -1)
		activeMenu = lastMenu;

	if(activeMenu != -1)
		toolList = menuList[activeMenu]->GetToolList();
	else
		toolList = std::vector<Tool*>();

	notifyMenuListChanged();
	notifyToolListChanged();
	notifyActiveToolsChanged();
	notifyLastToolChanged();
}

Tool * GameModel::GetToolFromIdentifier(ByteString identifier)
{
	for (std::vector<Menu*>::iterator iter = menuList.begin(), end = menuList.end(); iter != end; ++iter)
	{
		std::vector<Tool*> menuTools = (*iter)->GetToolList();
		for (std::vector<Tool*>::iterator titer = menuTools.begin(), tend = menuTools.end(); titer != tend; ++titer)
		{
			if (identifier == (*titer)->GetIdentifier())
				return *titer;
		}
	}
	for (std::vector<Tool*>::iterator iter = extraElementTools.begin(), end = extraElementTools.end(); iter != end; ++iter)
	{
		if (identifier == (*iter)->GetIdentifier())
			return *iter;
	}

	return NULL;
}

void GameModel::SetEdgeMode(int edgeMode)
{
	this->edgeMode = edgeMode;
	sim->SetEdgeMode(edgeMode);
}

int GameModel::GetEdgeMode()
{
	return this->edgeMode;
}

std::deque<Snapshot*> GameModel::GetHistory()
{
	return history;
}

unsigned int GameModel::GetHistoryPosition()
{
	return historyPosition;
}

void GameModel::SetHistory(std::deque<Snapshot*> newHistory)
{
	history = newHistory;
}

void GameModel::SetHistoryPosition(unsigned int newHistoryPosition)
{
	historyPosition = newHistoryPosition;
}

Snapshot * GameModel::GetRedoHistory()
{
	return redoHistory;
}

void GameModel::SetRedoHistory(Snapshot * redo)
{
	redoHistory = redo;
}

unsigned int GameModel::GetUndoHistoryLimit()
{
	return undoHistoryLimit;
}

void GameModel::SetUndoHistoryLimit(unsigned int undoHistoryLimit_)
{
	undoHistoryLimit = undoHistoryLimit_;
}

void GameModel::SetVote(int direction)
{
}

Brush * GameModel::GetBrush()
{
	return brushList[currentBrush];
}

vector<Brush*> GameModel::GetBrushList()
{
	return brushList;
}

int GameModel::GetBrushID()
{
	return currentBrush;
}

void GameModel::SetBrushID(int i)
{
	currentBrush = i%brushList.size();
	notifyBrushChanged();
}

void GameModel::AddObserver(GameView * observer){
	observers.push_back(observer);

	observer->NotifySimulationChanged(this);
	observer->NotifyRendererChanged(this);
	observer->NotifyPausedChanged(this);
	observer->NotifySaveChanged(this);
	observer->NotifyBrushChanged(this);
	observer->NotifyMenuListChanged(this);
	observer->NotifyToolListChanged(this);
	observer->NotifyZoomChanged(this);
	observer->NotifyColourSelectorVisibilityChanged(this);
	observer->NotifyColourSelectorColourChanged(this);
	observer->NotifyColourPresetsChanged(this);
	observer->NotifyColourActivePresetChanged(this);
	observer->NotifyQuickOptionsChanged(this);
	observer->NotifyLastToolChanged(this);
	UpdateQuickOptions();
}

void GameModel::SetToolStrength(float value)
{
	toolStrength = value;
}

float GameModel::GetToolStrength()
{
	return toolStrength;
}

void GameModel::SetActiveMenu(int menuID)
{
	activeMenu = menuID;
	toolList = menuList[menuID]->GetToolList();
	notifyToolListChanged();

	if(menuID == SC_DECO)
	{
		if(activeTools != decoToolset)
		{
			activeTools = decoToolset;
			notifyActiveToolsChanged();
		}
	}
	else
	{
		if(activeTools != regularToolset)
		{
			activeTools = regularToolset;
			notifyActiveToolsChanged();
		}
	}
}

vector<Tool*> GameModel::GetUnlistedTools()
{
	return extraElementTools;
}

vector<Tool*> GameModel::GetToolList()
{
	return toolList;
}

int GameModel::GetActiveMenu()
{
	return activeMenu;
}

//Get an element tool from an element ID
Tool * GameModel::GetElementTool(int elementID)
{
	for(std::vector<Tool*>::iterator iter = elementTools.begin(), end = elementTools.end(); iter != end; ++iter)
	{
		if((*iter)->GetToolID() == elementID)
			return *iter;
	}
	return NULL;
}

Tool * GameModel::GetActiveTool(int selection)
{
	return activeTools[selection];
}

void GameModel::SetActiveTool(int selection, Tool * tool)
{
	activeTools[selection] = tool;
	notifyActiveToolsChanged();
}

vector<QuickOption*> GameModel::GetQuickOptions()
{
	return quickOptions;
}

vector<Menu*> GameModel::GetMenuList()
{
	return menuList;
}

void GameModel::SetSaveFile(SaveFile * newSave)
{
}

Simulation * GameModel::GetSimulation()
{
	return sim;
}

Renderer * GameModel::GetRenderer()
{
	return ren;
}

Tool * GameModel::GetLastTool()
{
	return lastTool;
}

void GameModel::SetLastTool(Tool * newTool)
{
	if(lastTool != newTool)
	{
		lastTool = newTool;
		notifyLastToolChanged();
	}
}

void GameModel::SetZoomEnabled(bool enabled)
{
	ren->zoomEnabled = enabled;
	notifyZoomChanged();
}

bool GameModel::GetZoomEnabled()
{
	return ren->zoomEnabled;
}

void GameModel::SetZoomPosition(ui::Point position)
{
	ren->zoomScopePosition = position;
	notifyZoomChanged();
}

ui::Point GameModel::GetZoomPosition()
{
	return ren->zoomScopePosition;
}

bool GameModel::MouseInZoom(ui::Point position)
{
	if (!GetZoomEnabled())
		return false;

	int zoomFactor = GetZoomFactor();
	ui::Point zoomWindowPosition = GetZoomWindowPosition();
	ui::Point zoomWindowSize = ui::Point(GetZoomSize()*zoomFactor, GetZoomSize()*zoomFactor);

	if (position.X >= zoomWindowPosition.X && position.X >= zoomWindowPosition.Y && position.X <= zoomWindowPosition.X+zoomWindowSize.X && position.Y <= zoomWindowPosition.Y+zoomWindowSize.Y)
		return true;
	return false;
}

ui::Point GameModel::AdjustZoomCoords(ui::Point position)
{
	if (!GetZoomEnabled())
		return position;

	int zoomFactor = GetZoomFactor();
	ui::Point zoomWindowPosition = GetZoomWindowPosition();
	ui::Point zoomWindowSize = ui::Point(GetZoomSize()*zoomFactor, GetZoomSize()*zoomFactor);

	if (position.X >= zoomWindowPosition.X && position.X >= zoomWindowPosition.Y && position.X <= zoomWindowPosition.X+zoomWindowSize.X && position.Y <= zoomWindowPosition.Y+zoomWindowSize.Y)
		return ((position-zoomWindowPosition)/GetZoomFactor())+GetZoomPosition();
	return position;
}

void GameModel::SetZoomWindowPosition(ui::Point position)
{
	ren->zoomWindowPosition = position;
	notifyZoomChanged();
}

ui::Point GameModel::GetZoomWindowPosition()
{
	return ren->zoomWindowPosition;
}

void GameModel::SetZoomSize(int size)
{
	ren->zoomScopeSize = size;
	notifyZoomChanged();
}

int GameModel::GetZoomSize()
{
	return ren->zoomScopeSize;
}

void GameModel::SetZoomFactor(int factor)
{
	ren->ZFACTOR = factor;
	notifyZoomChanged();
}

int GameModel::GetZoomFactor()
{
	return ren->ZFACTOR;
}

void GameModel::SetActiveColourPreset(size_t preset)
{
	if (activeColourPreset-1 != preset)
		activeColourPreset = preset+1;
	else
	{
		activeTools[0] = GetToolFromIdentifier("DEFAULT_DECOR_SET");
		notifyActiveToolsChanged();
	}
	notifyColourActivePresetChanged();
}

size_t GameModel::GetActiveColourPreset()
{
	return activeColourPreset-1;
}

void GameModel::SetPresetColour(ui::Colour colour)
{
	if (activeColourPreset > 0 && activeColourPreset <= colourPresets.size())
	{
		colourPresets[activeColourPreset-1] = colour;
		notifyColourPresetsChanged();
	}
}

std::vector<ui::Colour> GameModel::GetColourPresets()
{
	return colourPresets;
}

void GameModel::SetColourSelectorVisibility(bool visibility)
{
	if(colourSelector != visibility)
	{
		colourSelector = visibility;
		notifyColourSelectorVisibilityChanged();
	}
}

bool GameModel::GetColourSelectorVisibility()
{
	return colourSelector;
}

void GameModel::SetColourSelectorColour(ui::Colour colour_)
{
	colour = colour_;

	vector<Tool*> tools = GetMenuList()[SC_DECO]->GetToolList();
	for (size_t i = 0; i < tools.size(); i++)
	{
		((DecorationTool*)tools[i])->Red = colour.Red;
		((DecorationTool*)tools[i])->Green = colour.Green;
		((DecorationTool*)tools[i])->Blue = colour.Blue;
		((DecorationTool*)tools[i])->Alpha = colour.Alpha;
	}

	notifyColourSelectorColourChanged();
}

ui::Colour GameModel::GetColourSelectorColour()
{
	return colour;
}

void GameModel::SetPaused(bool pauseState)
{
	if (!pauseState && sim->debug_currentParticle > 0)
	{
		String logmessage = String::Build("Updated particles from #", sim->debug_currentParticle, " to end due to unpause");
		sim->UpdateParticles(sim->debug_currentParticle, NPART);
		sim->AfterSim();
		sim->debug_currentParticle = 0;
		Log(logmessage, false);
	}

	sim->sys_pause = pauseState?1:0;
	notifyPausedChanged();
}

bool GameModel::GetPaused()
{
	return sim->sys_pause?true:false;
}

void GameModel::SetDecoration(bool decorationState)
{
	if (ren->decorations_enable != (decorationState?1:0))
	{
		ren->decorations_enable = decorationState?1:0;
		notifyDecorationChanged();
		UpdateQuickOptions();
		if (decorationState)
			SetInfoTip("Decorations Layer: On");
		else
			SetInfoTip("Decorations Layer: Off");
	}
}

bool GameModel::GetDecoration()
{
	return ren->decorations_enable?true:false;
}

void GameModel::SetAHeatEnable(bool aHeat)
{
	sim->aheat_enable = aHeat;
	UpdateQuickOptions();
	if (aHeat)
		SetInfoTip("Ambient Heat: On");
	else
		SetInfoTip("Ambient Heat: Off");
}

bool GameModel::GetAHeatEnable()
{
	return sim->aheat_enable;
}

void GameModel::SetNewtonianGravity(bool newtonainGravity)
{
    if (newtonainGravity)
    {
        sim->grav->start_grav_async();
        SetInfoTip("Newtonian Gravity: On");
    }
    else
    {
        sim->grav->stop_grav_async();
        SetInfoTip("Newtonian Gravity: Off");
    }
    UpdateQuickOptions();
}

bool GameModel::GetNewtonianGrvity()
{
    return sim->grav->ngrav_enable;
}

void GameModel::ShowGravityGrid(bool showGrid)
{
	ren->gravityFieldEnabled = showGrid;
	if (showGrid)
		SetInfoTip("Gravity Grid: On");
	else
		SetInfoTip("Gravity Grid: Off");
}

bool GameModel::GetGravityGrid()
{
	return ren->gravityFieldEnabled;
}

void GameModel::FrameStep(int frames)
{
	sim->framerender += frames;
}

void GameModel::ClearSimulation()
{
	//Load defaults
	sim->gravityMode = 0;
	sim->air->airMode = 0;
	sim->legacy_enable = false;
	sim->water_equal_test = false;
	sim->SetEdgeMode(edgeMode);

	sim->clear_sim();
	ren->ClearAccumulation();

	notifySaveChanged();
	UpdateQuickOptions();
}

void GameModel::Log(String message, bool printToFile)
{
	consoleLog.push_front(message);
	if(consoleLog.size()>100)
		consoleLog.pop_back();
	notifyLogChanged(message);
	if (printToFile)
		std::cout << message.ToUtf8() << std::endl;
}

deque<String> GameModel::GetLog()
{
	return consoleLog;
}

std::vector<Notification*> GameModel::GetNotifications()
{
	return notifications;
}

void GameModel::AddNotification(Notification * notification)
{
	notifications.push_back(notification);
	notifyNotificationsChanged();
}

void GameModel::RemoveNotification(Notification * notification)
{
	for(std::vector<Notification*>::iterator iter = notifications.begin(); iter != notifications.end(); ++iter)
	{
		if(*iter == notification)
		{
			delete *iter;
			notifications.erase(iter);
			break;
		}
	}
	notifyNotificationsChanged();
}

void GameModel::SetToolTip(String text)
{
	toolTip = text;
	notifyToolTipChanged();
}

void GameModel::SetInfoTip(String text)
{
	infoTip = text;
	notifyInfoTipChanged();
}

String GameModel::GetToolTip()
{
	return toolTip;
}

String GameModel::GetInfoTip()
{
	return infoTip;
}

void GameModel::notifyNotificationsChanged()
{
	for (std::vector<GameView*>::iterator iter = observers.begin(); iter != observers.end(); ++iter)
	{
		(*iter)->NotifyNotificationsChanged(this);
	}
}

void GameModel::notifyColourPresetsChanged()
{
	for (std::vector<GameView*>::iterator iter = observers.begin(); iter != observers.end(); ++iter)
	{
		(*iter)->NotifyColourPresetsChanged(this);
	}
}

void GameModel::notifyColourActivePresetChanged()
{
	for (std::vector<GameView*>::iterator iter = observers.begin(); iter != observers.end(); ++iter)
	{
		(*iter)->NotifyColourActivePresetChanged(this);
	}
}

void GameModel::notifyColourSelectorColourChanged()
{
	for (size_t i = 0; i < observers.size(); i++)
	{
		observers[i]->NotifyColourSelectorColourChanged(this);
	}
}

void GameModel::notifyColourSelectorVisibilityChanged()
{
	for (size_t i = 0; i < observers.size(); i++)
	{
		observers[i]->NotifyColourSelectorVisibilityChanged(this);
	}
}

void GameModel::notifyRendererChanged()
{
	for (size_t i = 0; i < observers.size(); i++)
	{
		observers[i]->NotifyRendererChanged(this);
	}
}

void GameModel::notifySaveChanged()
{
	for (size_t i = 0; i < observers.size(); i++)
	{
		observers[i]->NotifySaveChanged(this);
	}
}

void GameModel::notifySimulationChanged()
{
	for (size_t i = 0; i < observers.size(); i++)
	{
		observers[i]->NotifySimulationChanged(this);
	}
}

void GameModel::notifyPausedChanged()
{
	for (size_t i = 0; i < observers.size(); i++)
	{
		observers[i]->NotifyPausedChanged(this);
	}
}

void GameModel::notifyDecorationChanged()
{
	for (size_t i = 0; i < observers.size(); i++)
	{
		//observers[i]->NotifyPausedChanged(this);
	}
}

void GameModel::notifyBrushChanged()
{
	for (size_t i = 0; i < observers.size(); i++)
	{
		observers[i]->NotifyBrushChanged(this);
	}
}

void GameModel::notifyMenuListChanged()
{
	for (size_t i = 0; i < observers.size(); i++)
	{
		observers[i]->NotifyMenuListChanged(this);
	}
}

void GameModel::notifyToolListChanged()
{
	for (size_t i = 0; i < observers.size(); i++)
	{
		observers[i]->NotifyToolListChanged(this);
	}
}

void GameModel::notifyActiveToolsChanged()
{
	for (size_t i = 0; i < observers.size(); i++)
	{
		observers[i]->NotifyActiveToolsChanged(this);
	}
}

void GameModel::notifyUserChanged()
{
}

void GameModel::notifyZoomChanged()
{
	for (size_t i = 0; i < observers.size(); i++)
	{
		observers[i]->NotifyZoomChanged(this);
	}
}

void GameModel::notifyPlaceSaveChanged()
{
	for (size_t i = 0; i < observers.size(); i++)
	{
		observers[i]->NotifyPlaceSaveChanged(this);
	}
}

void GameModel::notifyLogChanged(String entry)
{
	for (size_t i = 0; i < observers.size(); i++)
	{
		observers[i]->NotifyLogChanged(this, entry);
	}
}

void GameModel::notifyInfoTipChanged()
{
	for (size_t i = 0; i < observers.size(); i++)
	{
		observers[i]->NotifyInfoTipChanged(this);
	}
}

void GameModel::notifyToolTipChanged()
{
	for (size_t i = 0; i < observers.size(); i++)
	{
		observers[i]->NotifyToolTipChanged(this);
	}
}

void GameModel::notifyQuickOptionsChanged()
{
	for (size_t i = 0; i < observers.size(); i++)
	{
		observers[i]->NotifyQuickOptionsChanged(this);
	}
}

void GameModel::notifyLastToolChanged()
{
	for (size_t i = 0; i < observers.size(); i++)
	{
		observers[i]->NotifyLastToolChanged(this);
	}
}
