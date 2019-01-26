#ifndef GAMECONTROLLER_H
#define GAMECONTROLLER_H

#include <queue>
#include "GameView.h"
#include "GameModel.h"
#include "simulation/Simulation.h"
#include "gui/interface/Point.h"
#include "gui/render/RenderController.h"
#include "gui/preview/PreviewController.h"
#include "gui/login/LoginController.h"
#include "gui/tags/TagsController.h"
#include "gui/console/ConsoleController.h"
#include "gui/options/OptionsController.h"
#include "client/ClientListener.h"
#include "RenderPreset.h"
#include "Menu.h"

using namespace std;

class DebugInfo;
class Notification;
class GameModel;
class GameView;
class CommandInterface;
class ConsoleController;
class GameController: public ClientListener
{
private:
	bool firstTick;
	int foundSignID;

	PreviewController * activePreview;
	GameView * gameView;
	GameModel * gameModel;
	RenderController * renderOptions;
	LoginController * loginWindow;
	ConsoleController * console;
	TagsController * tagsWindow;
	OptionsController * options;
	CommandInterface * commandInterface;
	vector<DebugInfo*> debugInfo;
	unsigned int debugFlags;
public:
	bool HasDone;
	class SearchCallback;
	class SSaveCallback;
	class TagsCallback;
	class StampsCallback;
	class OptionsCallback;
	class SaveOpenCallback;
	friend class SaveOpenCallback;
	GameController();
	~GameController();
	GameView * GetView();
	int GetSignAt(int x, int y);
	String GetSignText(int signID);

	bool MouseMove(int x, int y, int dx, int dy);
	bool MouseDown(int x, int y, unsigned button);
	bool MouseUp(int x, int y, unsigned button, char type);
	bool MouseWheel(int x, int y, int d);
	bool TextInput(String text);
	bool KeyPress(int key, int scan, bool repeat, bool shift, bool ctrl, bool alt);
	bool KeyRelease(int key, int scan, bool repeat, bool shift, bool ctrl, bool alt);
	void Tick();
	void Exit();

	void Install();

	void HistoryRestore();
	void HistorySnapshot();
	void HistoryForward();

	void AdjustGridSize(int direction);
	void InvertAirSim();
	void LoadRenderPreset(int presetNum);
	void SetZoomEnabled(bool zoomEnable);
	void SetZoomPosition(ui::Point position);
	void AdjustBrushSize(int direction, bool logarithmic = false, bool xAxis = false, bool yAxis = false);
	void SetBrushSize(ui::Point newSize);
	void AdjustZoomSize(int direction, bool logarithmic = false);
	void ToolClick(int toolSelection, ui::Point point);
	void DrawPoints(int toolSelection, ui::Point oldPos, ui::Point newPos, bool held);
	void DrawRect(int toolSelection, ui::Point point1, ui::Point point2);
	void DrawLine(int toolSelection, ui::Point point1, ui::Point point2);
	void DrawFill(int toolSelection, ui::Point point);
	ByteString StampRegion(ui::Point point1, ui::Point point2, bool includePressure);
	void CopyRegion(ui::Point point1, ui::Point point2, bool includePressure);
	void CutRegion(ui::Point point1, ui::Point point2, bool includePressure);
	void Update();
	void SetPaused(bool pauseState);
	void SetPaused();
	void SetDecoration(bool decorationState);
	void SetDecoration();
	void ShowGravityGrid();
	void SetHudEnable(bool hudState);
	bool GetHudEnable();
	void SetDebugHUD(bool hudState);
	bool GetDebugHUD();
	void SetDebugFlags(unsigned int flags) { debugFlags = flags; }
	void SetActiveMenu(int menuID);
	std::vector<Menu*> GetMenuList();
	int GetNumMenus(bool onlyEnabled);
	void RebuildFavoritesMenu();
	Tool * GetActiveTool(int selection);
	void SetActiveTool(int toolSelection, Tool * tool);
	void SetActiveTool(int toolSelection, ByteString identifier);
	void SetLastTool(Tool * tool);
	int GetReplaceModeFlags();
	void SetReplaceModeFlags(int flags);
	void SetActiveColourPreset(int preset);
	void SetColour(ui::Colour colour);
	void SetToolStrength(float value);
	void LoadSaveFile(SaveFile * file);
	void LoadSave(SaveInfo * save);
	void OpenSearch(String searchText);
	void OpenLogin();
	void OpenProfile();
	void OpenTags();
	void OpenSavePreview(int saveID, int saveDate, bool instant);
	void OpenSavePreview();
	void OpenLocalSaveWindow(bool asCurrent);
	void OpenLocalBrowse();
	void OpenOptions();
	void OpenRenderOptions();
	void OpenSaveWindow();
	void SaveAsCurrent();
	void OpenStamps();
	void OpenElementSearch();
	void OpenColourPicker();
	void PlaceSave(ui::Point position, bool includePressure);
	void ClearSim();
	void ReloadSim();
	void Vote(int direction);
	void ChangeBrush();
	void ShowConsole();
	void HideConsole();
	void FrameStep();
	void TranslateSave(ui::Point point);
	void TransformSave(matrix2d transform);
	bool MouseInZoom(ui::Point position);
	ui::Point PointTranslate(ui::Point point);
	ui::Point NormaliseBlockCoord(ui::Point point);
	ByteString ElementResolve(int type, int ctype);
	bool IsValidElement(int type);
	String WallName(int type);
	int Record(bool record);

	void ResetAir();
	void ResetSpark();
	void SwitchGravity();
	void SwitchAir();
	void ToggleAHeat();
	bool GetAHeatEnable();
	void ToggleNewtonianGravity();

	bool LoadClipboard();
	void LoadStamp(GameSave *stamp);

	void RemoveNotification(Notification * notification);

	virtual void NotifyUpdateAvailable(Client * sender);
	virtual void NotifyAuthUserChanged(Client * sender);
	virtual void NotifyNewNotification(Client * sender, std::pair<String, ByteString> notification);
	void RunUpdater();
};

#endif // GAMECONTROLLER_H
