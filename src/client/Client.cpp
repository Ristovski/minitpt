#include <cstdlib>
#include <iostream>
#include <vector>
#include <map>
#include <iomanip>
#include <ctime>
#include <cstdio>
#include <deque>
#include <fstream>
#include <dirent.h>

#ifdef MACOSX
#include <mach-o/dyld.h>
#include <ApplicationServices/ApplicationServices.h>
#endif

#ifdef WIN
#define NOMINMAX
#include <shlobj.h>
#include <shlwapi.h>
#include <windows.h>
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "common/String.h"
#include "Config.h"
#include "Format.h"
#include "Client.h"
#include "graphics/Graphics.h"
#include "Misc.h"
#include "Platform.h"

#include "simulation/SaveRenderer.h"
#include "gui/interface/Point.h"


extern "C"
{
#if defined(WIN) && !defined(__GNUC__)
#include <io.h>
#else
#include <dirent.h>
#endif
}


Client::Client():
	messageOfTheDay("Fetching the message of the day..."),
	versionCheckRequest(NULL),
	alternateVersionCheckRequest(NULL),
	usingAltUpdateServer(false),
	updateAvailable(false)
{
	firstRun = false;
}

void Client::Initialise(ByteString proxyString)
{
	//Read stamps library
	std::ifstream stampsLib;
	stampsLib.open(STAMPS_DIR PATH_SEP "stamps.def", std::ios::binary);
	while (!stampsLib.eof())
	{
		char data[11];
		memset(data, 0, 11);
		stampsLib.read(data, 10);
		if(!data[0])
			break;
		stampIDs.push_back(data);
	}
	stampsLib.close();
}

bool Client::IsFirstRun()
{
	return firstRun;
}

bool Client::DoInstallation()
{
}

void Client::SetProxy(ByteString proxy)
{
}

std::vector<ByteString> Client::DirectorySearch(ByteString directory, ByteString search, ByteString extension)
{
	std::vector<ByteString> extensions;
	extensions.push_back(extension);
	return DirectorySearch(directory, search.ToUpper(), extensions);
}

std::vector<ByteString> Client::DirectorySearch(ByteString directory, ByteString search, std::vector<ByteString> extensions)
{
	//Get full file listing
	//Normalise directory string, ensure / or \ is present
	if(*directory.rbegin() != '/' && *directory.rbegin() != '\\')
		directory += PATH_SEP;
	std::vector<ByteString> directoryList;
#if defined(WIN) && !defined(__GNUC__)
	//Windows
	struct _finddata_t currentFile;
	intptr_t findFileHandle;
	ByteString fileMatch = directory + "*.*";
	findFileHandle = _findfirst(fileMatch.c_str(), &currentFile);
	if (findFileHandle == -1L)
	{
#ifdef DEBUG
		printf("Unable to open directory: %s\n", directory.c_str());
#endif
		return std::vector<ByteString>();
	}
	do
	{
		ByteString currentFileName = ByteString(currentFile.name);
		if(currentFileName.length()>4)
			directoryList.push_back(directory+currentFileName);
	}
	while (_findnext(findFileHandle, &currentFile) == 0);
	_findclose(findFileHandle);
#else
	//Linux or MinGW
	struct dirent * directoryEntry;
	DIR *directoryHandle = opendir(directory.c_str());
	if(!directoryHandle)
	{
#ifdef DEBUG
		printf("Unable to open directory: %s\n", directory.c_str());
#endif
		return std::vector<ByteString>();
	}
	while ((directoryEntry = readdir(directoryHandle)))
	{
		ByteString currentFileName = ByteString(directoryEntry->d_name);
		if(currentFileName.length()>4)
			directoryList.push_back(directory+currentFileName);
	}
	closedir(directoryHandle);
#endif

	std::vector<ByteString> searchResults;
	for(std::vector<ByteString>::iterator iter = directoryList.begin(), end = directoryList.end(); iter != end; ++iter)
	{
		ByteString filename = *iter, tempfilename = *iter;
		bool extensionMatch = !extensions.size();
		for(std::vector<ByteString>::iterator extIter = extensions.begin(), extEnd = extensions.end(); extIter != extEnd; ++extIter)
		{
			if(filename.EndsWith(*extIter))
			{
				extensionMatch = true;
				tempfilename = filename.SubstrFromEnd(0, (*extIter).size()).ToUpper();
				break;
			}
		}
		bool searchMatch = !search.size();
		if(search.size() && tempfilename.Contains(search))
			searchMatch = true;

		if(searchMatch && extensionMatch)
			searchResults.push_back(filename);
	}

	//Filter results
	return searchResults;
}

int Client::MakeDirectory(const char * dirName)
{
#ifdef WIN
	return _mkdir(dirName);
#else
	return mkdir(dirName, 0755);
#endif
}

bool Client::WriteFile(std::vector<unsigned char> fileData, ByteString filename)
{
	bool saveError = false;
	try
	{
		std::ofstream fileStream;
		fileStream.open(filename, std::ios::binary);
		if(fileStream.is_open())
		{
			fileStream.write((char*)&fileData[0], fileData.size());
			fileStream.close();
		}
		else
			saveError = true;
	}
	catch (std::exception & e)
	{
		std::cerr << "WriteFile:" << e.what() << std::endl;
		saveError = true;
	}
	return saveError;
}

bool Client::FileExists(ByteString filename)
{
	bool exists = false;
	try
	{
		std::ifstream fileStream;
		fileStream.open(filename, std::ios::binary);
		if(fileStream.is_open())
		{
			exists = true;
			fileStream.close();
		}
	}
	catch (std::exception & e)
	{
		exists = false;
	}
	return exists;
}

bool Client::WriteFile(std::vector<char> fileData, ByteString filename)
{
	bool saveError = false;
	try
	{
		std::ofstream fileStream;
		fileStream.open(filename, std::ios::binary);
		if(fileStream.is_open())
		{
			fileStream.write(&fileData[0], fileData.size());
			fileStream.close();
		}
		else
			saveError = true;
	}
	catch (std::exception & e)
	{
		std::cerr << "WriteFile:" << e.what() << std::endl;
		saveError = true;
	}
	return saveError;
}

std::vector<unsigned char> Client::ReadFile(ByteString filename)
{
	try
	{
		std::ifstream fileStream;
		fileStream.open(filename, std::ios::binary);
		if(fileStream.is_open())
		{
			fileStream.seekg(0, std::ios::end);
			size_t fileSize = fileStream.tellg();
			fileStream.seekg(0);

			unsigned char * tempData = new unsigned char[fileSize];
			fileStream.read((char *)tempData, fileSize);
			fileStream.close();

			std::vector<unsigned char> fileData;
			fileData.insert(fileData.end(), tempData, tempData+fileSize);
			delete[] tempData;

			return fileData;
		}
		else
		{
			return std::vector<unsigned char>();
		}
	}
	catch(std::exception & e)
	{
		std::cerr << "Readfile: " << e.what() << std::endl;
		throw;
	}
}

void Client::SetMessageOfTheDay(String message)
{
	messageOfTheDay = message;
	notifyMessageOfTheDay();
}

String Client::GetMessageOfTheDay()
{
	return messageOfTheDay;
}

void Client::AddServerNotification(std::pair<String, ByteString> notification)
{
	serverNotifications.push_back(notification);
	notifyNewNotification(notification);
}

std::vector<std::pair<String, ByteString> > Client::GetServerNotifications()
{
	return serverNotifications;
}

void Client::Tick()
{
}

bool Client::CheckUpdate(void *updateRequest, bool checkSession)
{
}

UpdateInfo Client::GetUpdateInfo()
{
	return updateInfo;
}

void Client::notifyUpdateAvailable()
{
}

void Client::notifyMessageOfTheDay()
{
}

void Client::notifyAuthUserChanged()
{
}

void Client::notifyNewNotification(std::pair<String, ByteString> notification)
{
}

void Client::WritePrefs()
{
}

void Client::Shutdown()
{
	//Save config
	WritePrefs();
}

Client::~Client()
{
}

void Client::MoveStampToFront(ByteString stampID)
{
}

SaveFile * Client::GetStamp(ByteString stampID)
{
}

void Client::DeleteStamp(ByteString stampID)
{
	for (std::list<ByteString>::iterator iterator = stampIDs.begin(), end = stampIDs.end(); iterator != end; ++iterator)
	{
		if((*iterator) == stampID)
		{
			ByteString stampFilename = ByteString::Build(STAMPS_DIR, PATH_SEP, stampID, ".stm");
			remove(stampFilename.c_str());
			stampIDs.erase(iterator);
			return;
		}
	}

	updateStamps();
}

void Client::updateStamps()
{
}

void Client::RescanStamps()
{
}

int Client::GetStampsCount()
{
}

std::vector<ByteString> Client::GetStamps(int start, int count)
{
}

unsigned char * Client::GetSaveData(int saveID, int saveDate, int & dataLength)
{
}

std::vector<unsigned char> Client::GetSaveData(int saveID, int saveDate)
{
}

std::vector<std::pair<ByteString, int> > * Client::GetTags(int start, int count, String query, int & resultCount)
{
}

std::list<ByteString> * Client::RemoveTag(int saveID, ByteString tag)
{
}

std::list<ByteString> * Client::AddTag(int saveID, ByteString tag)
{
}

ByteString Client::GetPrefByteString(ByteString prop, ByteString defaultValue)
{
}

String Client::GetPrefString(ByteString prop, String defaultValue)
{
}

double Client::GetPrefNumber(ByteString prop, double defaultValue)
{
}

int Client::GetPrefInteger(ByteString prop, int defaultValue)
{
}

unsigned int Client::GetPrefUInteger(ByteString prop, unsigned int defaultValue)
{
}

bool Client::GetPrefBool(ByteString prop, bool defaultValue)
{
}

std::vector<ByteString> Client::GetPrefByteStringArray(ByteString prop)
{
}

std::vector<String> Client::GetPrefStringArray(ByteString prop)
{
}

std::vector<double> Client::GetPrefNumberArray(ByteString prop)
{
}

std::vector<int> Client::GetPrefIntegerArray(ByteString prop)
{
}

std::vector<unsigned int> Client::GetPrefUIntegerArray(ByteString prop)
{
}

std::vector<bool> Client::GetPrefBoolArray(ByteString prop)
{
}

void Client::SetPrefUnicode(ByteString prop, String value)
{
}
