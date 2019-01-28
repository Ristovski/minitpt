#ifndef CLIENT_H
#define CLIENT_H

#include <queue>
#include <vector>
#include <list>

#include "common/String.h"
#include "Config.h"
#include "common/Singleton.h"

class SaveInfo;
class SaveFile;
class SaveComment;
class GameSave;
class VideoBuffer;

enum LoginStatus {
	LoginOkay, LoginError
};

class UpdateInfo
{
public:
	enum BuildType { Stable, Beta, Snapshot };
	ByteString File;
	String Changelog;
	int Major;
	int Minor;
	int Build;
	int Time;
	BuildType Type;
	UpdateInfo() : File(""), Changelog(""), Major(0), Minor(0), Build(0), Time(0), Type(Stable) {}
	UpdateInfo(int major, int minor, int build, ByteString file, String changelog, BuildType type) : File(file), Changelog(changelog), Major(major), Minor(minor), Build(build), Time(0), Type(type) {}
	UpdateInfo(int time, ByteString file, String changelog, BuildType type) : File(file), Changelog(changelog), Major(0), Minor(0), Build(0), Time(time), Type(type) {}
};

class Client: public Singleton<Client> {
private:
	String messageOfTheDay;
	std::vector<std::pair<String, ByteString> > serverNotifications;

	void * versionCheckRequest;
	void * alternateVersionCheckRequest;
	bool usingAltUpdateServer;
	bool updateAvailable;
	UpdateInfo updateInfo;

	String lastError;
	bool firstRun;

	std::list<ByteString> stampIDs;
	unsigned lastStampTime;
	int lastStampName;

	void notifyUpdateAvailable();
	void notifyAuthUserChanged();
	void notifyMessageOfTheDay();
	void notifyNewNotification(std::pair<String, ByteString> notification);

public:

	UpdateInfo GetUpdateInfo();

	Client();
	~Client();

	std::vector<ByteString> DirectorySearch(ByteString directory, ByteString search, std::vector<ByteString> extensions);
	std::vector<ByteString> DirectorySearch(ByteString directory, ByteString search, ByteString extension);

	ByteString FileOpenDialogue();
	//std::string FileSaveDialogue();

	bool DoInstallation();

	std::vector<unsigned char> ReadFile(ByteString filename);

	void AddServerNotification(std::pair<String, ByteString> notification);
	std::vector<std::pair<String, ByteString> > GetServerNotifications();

	void SetMessageOfTheDay(String message);
	String GetMessageOfTheDay();

	void Initialise(ByteString proxyString);
	void SetProxy(ByteString proxy);
	bool IsFirstRun();

	int MakeDirectory(const char * dirname);
	bool WriteFile(std::vector<unsigned char> fileData, ByteString filename);
	bool WriteFile(std::vector<char> fileData, ByteString filename);
	bool FileExists(ByteString filename);

	SaveFile * GetStamp(ByteString stampID);
	void DeleteStamp(ByteString stampID);
	ByteString AddStamp(GameSave * saveData);
	std::vector<ByteString> GetStamps(int start, int count);
	void RescanStamps();
	int GetStampsCount();
	SaveFile * GetFirstStamp();
	void MoveStampToFront(ByteString stampID);
	void updateStamps();

	unsigned char * GetSaveData(int saveID, int saveDate, int & dataLength);
	std::vector<unsigned char> GetSaveData(int saveID, int saveDate);

	std::vector<SaveInfo*> * SearchSaves(int start, int count, String query, ByteString sort, ByteString category, int & resultCount);
	std::vector<std::pair<ByteString, int> > * GetTags(int start, int count, String query, int & resultCount);

	SaveInfo * GetSave(int saveID, int saveDate);

	std::list<ByteString> * RemoveTag(int saveID, ByteString tag); //TODO RequestStatus
	std::list<ByteString> * AddTag(int saveID, ByteString tag);
	String GetLastError() {
		return lastError;
	}
	void Tick();
	bool CheckUpdate(void *updateRequest, bool checkSession);
	void Shutdown();

	// preferences functions
	void WritePrefs();

	ByteString GetPrefByteString(ByteString prop, ByteString defaultValue);
	String GetPrefString(ByteString prop, String defaultValue);
	double GetPrefNumber(ByteString prop, double defaultValue);
	int GetPrefInteger(ByteString prop, int defaultValue);
	unsigned int GetPrefUInteger(ByteString prop, unsigned int defaultValue);
	bool GetPrefBool(ByteString prop, bool defaultValue);
	std::vector<ByteString> GetPrefByteStringArray(ByteString prop);
	std::vector<String> GetPrefStringArray(ByteString prop);
	std::vector<double> GetPrefNumberArray(ByteString prop);
	std::vector<int> GetPrefIntegerArray(ByteString prop);
	std::vector<unsigned int> GetPrefUIntegerArray(ByteString prop);
	std::vector<bool> GetPrefBoolArray(ByteString prop);

	void SetPrefUnicode(ByteString prop, String value);
};

#endif // CLIENT_H
