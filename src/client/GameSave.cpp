#include "common/tpt-minmax.h"
#include <iostream>
#include <cmath>
#include <climits>
#include <memory>
#include <vector>
#include <set>
#include <bzlib.h>
#include "Config.h"
#include "Format.h"
#include "GameSave.h"
#include "simulation/SimulationData.h"
#include "ElementClasses.h"
#include "hmap.h"

GameSave::GameSave(GameSave & save):
    majorVersion(save.majorVersion),
	waterEEnabled(save.waterEEnabled),
	legacyEnable(save.legacyEnable),
	gravityEnable(save.gravityEnable),
	aheatEnable(save.aheatEnable),
	paused(save.paused),
	gravityMode(save.gravityMode),
	airMode(save.airMode),
	edgeMode(save.edgeMode),
	signs(save.signs),
	stkm(save.stkm),
	palette(save.palette),
	pmapbits(save.pmapbits),
	expanded(save.expanded),
	hasOriginalData(save.hasOriginalData),
	originalData(save.originalData)
{
	InitData();
	hasPressure = save.hasPressure;
	hasAmbientHeat = save.hasAmbientHeat;
	if (save.expanded)
	{
		setSize(save.blockWidth, save.blockHeight);

		std::copy(save.particles, save.particles+NPART, particles);
		for (int j = 0; j < blockHeight; j++)
		{
			std::copy(save.blockMap[j], save.blockMap[j]+blockWidth, blockMap[j]);
			std::copy(save.fanVelX[j], save.fanVelX[j]+blockWidth, fanVelX[j]);
			std::copy(save.fanVelY[j], save.fanVelY[j]+blockWidth, fanVelY[j]);
			std::copy(save.pressure[j], save.pressure[j]+blockWidth, pressure[j]);
			std::copy(save.velocityX[j], save.velocityX[j]+blockWidth, velocityX[j]);
			std::copy(save.velocityY[j], save.velocityY[j]+blockWidth, velocityY[j]);
			std::copy(save.ambientHeat[j], save.ambientHeat[j]+blockWidth, ambientHeat[j]);
		}
	}
	else
	{
		blockWidth = save.blockWidth;
		blockHeight = save.blockHeight;
	}
	particlesCount = save.particlesCount;
	authors = save.authors;
}

GameSave::GameSave(int width, int height)
{
	InitData();
	InitVars();
	hasOriginalData = false;
	expanded = true;
	setSize(width, height);
}

GameSave::GameSave(std::vector<char> data)
{
	blockWidth = 0;
	blockHeight = 0;

	InitData();
	InitVars();
	expanded = false;
	hasOriginalData = true;
	originalData = data;
	try
	{
		Expand();
	}
	catch(ParseException & e)
	{
		std::cout << e.what() << std::endl;
		dealloc();	//Free any allocated memory
		throw;
	}
	Collapse();
}

GameSave::GameSave(std::vector<unsigned char> data)
{
	blockWidth = 0;
	blockHeight = 0;

	InitData();
	InitVars();
	expanded = false;
	hasOriginalData = true;
	originalData = std::vector<char>(data.begin(), data.end());
	try
	{
		Expand();
	}
	catch(ParseException & e)
	{
		std::cout << e.what() << std::endl;
		dealloc();	//Free any allocated memory
		throw;
	}
	Collapse();
}

GameSave::GameSave(char * data, int dataSize)
{
	blockWidth = 0;
	blockHeight = 0;

	InitData();
	InitVars();
	expanded = false;
	hasOriginalData = true;
	originalData = std::vector<char>(data, data+dataSize);
#ifdef DEBUG
	std::cout << "Creating Expanded save from data" << std::endl;
#endif
	try
	{
		Expand();
	}
	catch(ParseException & e)
	{
		std::cout << e.what() << std::endl;
		dealloc();	//Free any allocated memory
		throw;
	}
	//Collapse();
}

// Called on every new GameSave, including the copy constructor
void GameSave::InitData()
{
	blockMap = NULL;
	fanVelX = NULL;
	fanVelY = NULL;
	particles = NULL;
	pressure = NULL;
	velocityX = NULL;
	velocityY = NULL;
	ambientHeat = NULL;
	fromNewerVersion = false;
	hasPressure = false;
	hasAmbientHeat = false;
	authors.clear();
}

// Called on every new GameSave, except the copy constructor
void GameSave::InitVars()
{
	majorVersion = 0;
	waterEEnabled = false;
	legacyEnable = false;
	gravityEnable = false;
	aheatEnable = false;
	paused = false;
	gravityMode = 0;
	airMode = 0;
	edgeMode = 0;
	translated.x = translated.y = 0;
	pmapbits = 8; // default to 8 bits for older saves
}

bool GameSave::Collapsed()
{
	return !expanded;
}

void GameSave::Expand()
{
	if(hasOriginalData && !expanded)
	{
		InitVars();
		expanded = true;
		read(&originalData[0], originalData.size());
	}
}

void GameSave::Collapse()
{
	if(expanded && hasOriginalData)
	{
		expanded = false;
		dealloc();
		signs.clear();
	}
}

void GameSave::read(char * data, int dataSize)
{
	if(dataSize > 15)
	{
		if ((data[0]==0x66 && data[1]==0x75 && data[2]==0x43) || (data[0]==0x50 && data[1]==0x53 && data[2]==0x76))
		{
			readPSv(data, dataSize);
		}
		else if(data[0] == 'O' && data[1] == 'P' && data[2] == 'S')
		{
			if (data[3] != '1')
				throw ParseException(ParseException::WrongVersion, "Save format from newer version");
			readOPS(data, dataSize);
		}
		else
		{
			std::cerr << "Got Magic number '" << data[0] << data[1] << data[2] << "'" << std::endl;
			throw ParseException(ParseException::Corrupt, "Invalid save format");
		}
	}
	else
	{
		throw ParseException(ParseException::Corrupt, "No data");
	}
}

template <typename T>
T ** GameSave::Allocate2DArray(int blockWidth, int blockHeight, T defaultVal)
{
	T ** temp = new T*[blockHeight];
	for (int y = 0; y < blockHeight; y++)
	{
		temp[y] = new T[blockWidth];
		std::fill(&temp[y][0], &temp[y][0]+blockWidth, defaultVal);
	}
	return temp;
}

void GameSave::setSize(int newWidth, int newHeight)
{
	this->blockWidth = newWidth;
	this->blockHeight = newHeight;

	particlesCount = 0;
	particles = new Particle[NPART];

	blockMap = Allocate2DArray<unsigned char>(blockWidth, blockHeight, 0);
	fanVelX = Allocate2DArray<float>(blockWidth, blockHeight, 0.0f);
	fanVelY = Allocate2DArray<float>(blockWidth, blockHeight, 0.0f);
	pressure = Allocate2DArray<float>(blockWidth, blockHeight, 0.0f);
	velocityX = Allocate2DArray<float>(blockWidth, blockHeight, 0.0f);
	velocityY = Allocate2DArray<float>(blockWidth, blockHeight, 0.0f);
	ambientHeat = Allocate2DArray<float>(blockWidth, blockHeight, 0.0f);
}

std::vector<char> GameSave::Serialise()
{
	unsigned int dataSize;
	char * data = Serialise(dataSize);
	if (data == NULL)
		return std::vector<char>();
	std::vector<char> dataVect(data, data+dataSize);
	delete[] data;
	return dataVect;
}

char * GameSave::Serialise(unsigned int & dataSize)
{
	try
	{
		return serialiseOPS(dataSize);
	}
	catch (BuildException & e)
	{
		std::cout << e.what() << std::endl;
		return NULL;
	}
}

vector2d GameSave::Translate(vector2d translate)
{
	if (Collapsed())
		Expand();
	int nx, ny;
	vector2d pos;
	vector2d translateReal = translate;
	float minx = 0, miny = 0, maxx = 0, maxy = 0;
	// determine minimum and maximum position of all particles / signs
	for (size_t i = 0; i < signs.size(); i++)
	{
		pos = v2d_new(signs[i].x, signs[i].y);
		pos = v2d_add(pos,translate);
		nx = floor(pos.x+0.5f);
		ny = floor(pos.y+0.5f);
		if (nx < minx)
			minx = nx;
		if (ny < miny)
			miny = ny;
		if (nx > maxx)
			maxx = nx;
		if (ny > maxy)
			maxy = ny;
	}
	for (int i = 0; i < particlesCount; i++)
	{
		if (!particles[i].type) continue;
		pos = v2d_new(particles[i].x, particles[i].y);
		pos = v2d_add(pos,translate);
		nx = floor(pos.x+0.5f);
		ny = floor(pos.y+0.5f);
		if (nx < minx)
			minx = nx;
		if (ny < miny)
			miny = ny;
		if (nx > maxx)
			maxx = nx;
		if (ny > maxy)
			maxy = ny;
	}
	// determine whether corrections are needed. If moving in this direction would delete stuff, expand the save
	vector2d backCorrection = v2d_new(
		(minx < 0) ? (-floor(minx / CELL)) : 0,
		(miny < 0) ? (-floor(miny / CELL)) : 0
	);
	int blockBoundsX = int(maxx / CELL) + 1, blockBoundsY = int(maxy / CELL) + 1;
	vector2d frontCorrection = v2d_new(
		(blockBoundsX > blockWidth) ? (blockBoundsX - blockWidth) : 0,
		(blockBoundsY > blockHeight) ? (blockBoundsY - blockHeight) : 0
	);

	// get new width based on corrections
	int newWidth = (blockWidth + backCorrection.x + frontCorrection.x) * CELL;
	int newHeight = (blockHeight + backCorrection.y + frontCorrection.y) * CELL;
	if (newWidth > XRES)
		frontCorrection.x = backCorrection.x = 0;
	if (newHeight > YRES)
		frontCorrection.y = backCorrection.y = 0;

	// call Transform to do the transformation we wanted when calling this function
	translate = v2d_add(translate, v2d_multiply_float(backCorrection, CELL));
	Transform(m2d_identity, translate, translateReal,
	    (blockWidth + backCorrection.x + frontCorrection.x) * CELL,
	    (blockHeight + backCorrection.y + frontCorrection.y) * CELL
	);

	// return how much we corrected. This is used to offset the position of the current stamp
	// otherwise it would attempt to recenter it with the current size
	return v2d_add(v2d_multiply_float(backCorrection, -CELL), v2d_multiply_float(frontCorrection, CELL));
}

void GameSave::Transform(matrix2d transform, vector2d translate)
{
	if (Collapsed())
		Expand();

	int width = blockWidth*CELL, height = blockHeight*CELL, newWidth, newHeight;
	vector2d tmp, ctl, cbr;
	vector2d cornerso[4];
	vector2d translateReal = translate;
	// undo any translation caused by rotation
	cornerso[0] = v2d_new(0,0);
	cornerso[1] = v2d_new(width-1,0);
	cornerso[2] = v2d_new(0,height-1);
	cornerso[3] = v2d_new(width-1,height-1);
	for (int i = 0; i < 4; i++)
	{
		tmp = m2d_multiply_v2d(transform,cornerso[i]);
		if (i==0) ctl = cbr = tmp; // top left, bottom right corner
		if (tmp.x<ctl.x) ctl.x = tmp.x;
		if (tmp.y<ctl.y) ctl.y = tmp.y;
		if (tmp.x>cbr.x) cbr.x = tmp.x;
		if (tmp.y>cbr.y) cbr.y = tmp.y;
	}
	// casting as int doesn't quite do what we want with negative numbers, so use floor()
	tmp = v2d_new(floor(ctl.x+0.5f),floor(ctl.y+0.5f));
	translate = v2d_sub(translate,tmp);
	newWidth = floor(cbr.x+0.5f)-floor(ctl.x+0.5f)+1;
	newHeight = floor(cbr.y+0.5f)-floor(ctl.y+0.5f)+1;
	Transform(transform, translate, translateReal, newWidth, newHeight);
}

// transform is a matrix describing how we want to rotate this save
// translate can vary depending on whether the save is bring rotated, or if a normal translate caused it to expand
// translateReal is the original amount we tried to translate, used to calculate wall shifting
void GameSave::Transform(matrix2d transform, vector2d translate, vector2d translateReal, int newWidth, int newHeight)
{
	if (Collapsed())
		Expand();

	if (newWidth>XRES) newWidth = XRES;
	if (newHeight>YRES) newHeight = YRES;

	int x, y, nx, ny, newBlockWidth = newWidth / CELL, newBlockHeight = newHeight / CELL;
	vector2d pos, vel;

	unsigned char ** blockMapNew;
	float **fanVelXNew, **fanVelYNew, **pressureNew, **velocityXNew, **velocityYNew, **ambientHeatNew;

	blockMapNew = Allocate2DArray<unsigned char>(newBlockWidth, newBlockHeight, 0);
	fanVelXNew = Allocate2DArray<float>(newBlockWidth, newBlockHeight, 0.0f);
	fanVelYNew = Allocate2DArray<float>(newBlockWidth, newBlockHeight, 0.0f);
	pressureNew = Allocate2DArray<float>(newBlockWidth, newBlockHeight, 0.0f);
	velocityXNew = Allocate2DArray<float>(newBlockWidth, newBlockHeight, 0.0f);
	velocityYNew = Allocate2DArray<float>(newBlockWidth, newBlockHeight, 0.0f);
	ambientHeatNew = Allocate2DArray<float>(newBlockWidth, newBlockHeight, 0.0f);

	// rotate and translate signs, parts, walls
	for (size_t i = 0; i < signs.size(); i++)
	{
		pos = v2d_new(signs[i].x, signs[i].y);
		pos = v2d_add(m2d_multiply_v2d(transform,pos),translate);
		nx = floor(pos.x+0.5f);
		ny = floor(pos.y+0.5f);
		if (nx<0 || nx>=newWidth || ny<0 || ny>=newHeight)
		{
			signs[i].text[0] = 0;
			continue;
		}
		signs[i].x = nx;
		signs[i].y = ny;
	}
	for (int i = 0; i < particlesCount; i++)
	{
		if (!particles[i].type) continue;
		pos = v2d_new(particles[i].x, particles[i].y);
		pos = v2d_add(m2d_multiply_v2d(transform,pos),translate);
		nx = floor(pos.x+0.5f);
		ny = floor(pos.y+0.5f);
		if (nx<0 || nx>=newWidth || ny<0 || ny>=newHeight)
		{
			particles[i].type = PT_NONE;
			continue;
		}
		particles[i].x = nx;
		particles[i].y = ny;
		vel = v2d_new(particles[i].vx, particles[i].vy);
		vel = m2d_multiply_v2d(transform, vel);
		particles[i].vx = vel.x;
		particles[i].vy = vel.y;
	}

	// translate walls and other grid items when the stamp is shifted more than 4 pixels in any direction
	int translateX = 0, translateY = 0;
	if (translateReal.x > 0 && ((int)translated.x%CELL == 3
	                        || (translated.x < 0 && (int)translated.x%CELL == 0)))
		translateX = CELL;
	else if (translateReal.x < 0 && ((int)translated.x%CELL == -3
	                             || (translated.x > 0 && (int)translated.x%CELL == 0)))
		translateX = -CELL;
	if (translateReal.y > 0 && ((int)translated.y%CELL == 3
	                        || (translated.y < 0 && (int)translated.y%CELL == 0)))
		translateY = CELL;
	else if (translateReal.y < 0 && ((int)translated.y%CELL == -3
	                             || (translated.y > 0 && (int)translated.y%CELL == 0)))
		translateY = -CELL;

	for (y=0; y<blockHeight; y++)
		for (x=0; x<blockWidth; x++)
		{
			pos = v2d_new(x*CELL+CELL*0.4f+translateX, y*CELL+CELL*0.4f+translateY);
			pos = v2d_add(m2d_multiply_v2d(transform,pos),translate);
			nx = pos.x/CELL;
			ny = pos.y/CELL;
			if (pos.x<0 || nx>=newBlockWidth || pos.y<0 || ny>=newBlockHeight)
				continue;
			if (blockMap[y][x])
			{
				blockMapNew[ny][nx] = blockMap[y][x];
				if (blockMap[y][x]==WL_FAN)
				{
					vel = v2d_new(fanVelX[y][x], fanVelY[y][x]);
					vel = m2d_multiply_v2d(transform, vel);
					fanVelXNew[ny][nx] = vel.x;
					fanVelYNew[ny][nx] = vel.y;
				}
			}
			pressureNew[ny][nx] = pressure[y][x];
			velocityXNew[ny][nx] = velocityX[y][x];
			velocityYNew[ny][nx] = velocityY[y][x];
			ambientHeatNew[ny][nx] = ambientHeat[y][x];
		}
	translated = v2d_add(m2d_multiply_v2d(transform, translated), translateReal);

	for (int j = 0; j < blockHeight; j++)
	{
		delete[] blockMap[j];
		delete[] fanVelX[j];
		delete[] fanVelY[j];
		delete[] pressure[j];
		delete[] velocityX[j];
		delete[] velocityY[j];
		delete[] ambientHeat[j];
	}

	blockWidth = newBlockWidth;
	blockHeight = newBlockHeight;

	delete[] blockMap;
	delete[] fanVelX;
	delete[] fanVelY;
	delete[] pressure;
	delete[] velocityX;
	delete[] velocityY;
	delete[] ambientHeat;

	blockMap = blockMapNew;
	fanVelX = fanVelXNew;
	fanVelY = fanVelYNew;
	pressure = pressureNew;
	velocityX = velocityXNew;
	velocityY = velocityYNew;
	ambientHeat = ambientHeatNew;
}

void GameSave::CheckBsonFieldUser(bson_iterator iter, const char *field, unsigned char **data, unsigned int *fieldLen)
{
	if (!strcmp(bson_iterator_key(&iter), field))
	{
		if (bson_iterator_type(&iter)==BSON_BINDATA && ((unsigned char)bson_iterator_bin_type(&iter))==BSON_BIN_USER && (*fieldLen = bson_iterator_bin_len(&iter)) > 0)
		{
			*data = (unsigned char*)bson_iterator_bin_data(&iter);
		}
		else
		{
			fprintf(stderr, "Invalid datatype for %s: %d[%d] %d[%d] %d[%d]\n", field, bson_iterator_type(&iter), bson_iterator_type(&iter)==BSON_BINDATA, (unsigned char)bson_iterator_bin_type(&iter), ((unsigned char)bson_iterator_bin_type(&iter))==BSON_BIN_USER, bson_iterator_bin_len(&iter), bson_iterator_bin_len(&iter)>0);
		}
	}
}

void GameSave::CheckBsonFieldBool(bson_iterator iter, const char *field, bool *flag)
{
	if (!strcmp(bson_iterator_key(&iter), field))
	{
		if (bson_iterator_type(&iter) == BSON_BOOL)
		{
			*flag = bson_iterator_bool(&iter);
		}
		else
		{
			fprintf(stderr, "Wrong type for %s\n", bson_iterator_key(&iter));
		}
	}
}

void GameSave::CheckBsonFieldInt(bson_iterator iter, const char *field, int *setting)
{
	if (!strcmp(bson_iterator_key(&iter), field))
	{
		if (bson_iterator_type(&iter) == BSON_INT)
		{
			*setting = bson_iterator_int(&iter);
		}
		else
		{
			fprintf(stderr, "Wrong type for %s\n", bson_iterator_key(&iter));
		}
	}
}

void GameSave::readOPS(char * data, int dataLength)
{
}

void GameSave::readPSv(char * saveDataChar, int dataLength)
{
	unsigned char * saveData = (unsigned char *)saveDataChar;
	int q,j,k,x,y,p=0, ver, pty, ty, legacy_beta=0;
	int bx0=0, by0=0, bw, bh, w, h, y0 = 0, x0 = 0;
	int new_format = 0, ttv = 0;

	std::vector<sign> tempSigns;
	char tempSignText[255];
	sign tempSign("", 0, 0, sign::Left);

	//Gol data used to read older saves
	std::vector<int> goltype = LoadGOLTypes();
	std::vector<std::array<int, 10> > grule = LoadGOLRules();

	std::vector<Element> elements = GetElements();

	//New file header uses PSv, replacing fuC. This is to detect if the client uses a new save format for temperatures
	//This creates a problem for old clients, that display and "corrupt" error instead of a "newer version" error

	if (dataLength<16)
		throw ParseException(ParseException::Corrupt, "No save data");
	if (!(saveData[2]==0x43 && saveData[1]==0x75 && saveData[0]==0x66) && !(saveData[2]==0x76 && saveData[1]==0x53 && saveData[0]==0x50))
		throw ParseException(ParseException::Corrupt, "Unknown format");
	if (saveData[2]==0x76 && saveData[1]==0x53 && saveData[0]==0x50) {
		new_format = 1;
	}
	if (saveData[4]>SAVE_VERSION)
		throw ParseException(ParseException::WrongVersion, "Save from newer version");
	ver = saveData[4];
	majorVersion = saveData[4];

	if (ver<34)
	{
		legacyEnable = 1;
	}
	else
	{
		if (ver>=44) {
			legacyEnable = saveData[3]&0x01;
			paused = (saveData[3]>>1)&0x01;
			if (ver>=46) {
				gravityMode = ((saveData[3]>>2)&0x03);// | ((c[3]>>2)&0x01);
				airMode = ((saveData[3]>>4)&0x07);// | ((c[3]>>4)&0x02) | ((c[3]>>4)&0x01);
			}
			if (ver>=49) {
				gravityEnable = ((saveData[3]>>7)&0x01);
			}
		} else {
			if (saveData[3]==1||saveData[3]==0) {
				legacyEnable = saveData[3];
			} else {
				legacy_beta = 1;
			}
		}
	}

	bw = saveData[6];
	bh = saveData[7];
	if (bx0+bw > XRES/CELL)
		bx0 = XRES/CELL - bw;
	if (by0+bh > YRES/CELL)
		by0 = YRES/CELL - bh;
	if (bx0 < 0)
		bx0 = 0;
	if (by0 < 0)
		by0 = 0;

	if (saveData[5]!=CELL || bx0+bw>XRES/CELL || by0+bh>YRES/CELL)
		throw ParseException(ParseException::InvalidDimensions, "Save too large");
	int size = (unsigned)saveData[8];
	size |= ((unsigned)saveData[9])<<8;
	size |= ((unsigned)saveData[10])<<16;
	size |= ((unsigned)saveData[11])<<24;

	if (size > 209715200 || !size)
		throw ParseException(ParseException::InvalidDimensions, "Save data too large");

	auto dataPtr = std::unique_ptr<unsigned char[]>(new unsigned char[size]);
	unsigned char *data = dataPtr.get();
	if (!data)
		throw ParseException(ParseException::Corrupt, "Cannot allocate memory");

	setSize(bw, bh);

	int bzStatus = 0;
	if ((bzStatus = BZ2_bzBuffToBuffDecompress((char *)data, (unsigned *)&size, (char *)(saveData+12), dataLength-12, 0, 0)))
		throw ParseException(ParseException::Corrupt, String::Build("Cannot decompress: ", bzStatus));
	dataLength = size;

#ifdef DEBUG
	std::cout << "Parsing " << dataLength << " bytes of data, version " << ver << std::endl;
#endif

	if (dataLength < bw*bh)
		throw ParseException(ParseException::Corrupt, "Save data corrupt (missing data)");

	// normalize coordinates
	x0 = bx0*CELL;
	y0 = by0*CELL;
	w  = bw *CELL;
	h  = bh *CELL;

	if (ver<46) {
		gravityMode = 0;
		airMode = 0;
	}

	auto particleIDMapPtr = std::unique_ptr<int[]>(new int[XRES*YRES]);
	int *particleIDMap = particleIDMapPtr.get();
	std::fill(&particleIDMap[0], &particleIDMap[XRES*YRES], 0);
	if (!particleIDMap)
		throw ParseException(ParseException::Corrupt, "Cannot allocate memory");

	// load the required air state
	for (y=by0; y<by0+bh; y++)
		for (x=bx0; x<bx0+bw; x++)
		{
			if (data[p])
			{
				//In old saves, ignore walls created by sign tool bug
				//Not ignoring other invalid walls or invalid walls in new saves, so that any other bugs causing them are easier to notice, find and fix
				if (ver>=44 && ver<71 && data[p]==O_WL_SIGN)
				{
					p++;
					continue;
				}
				blockMap[y][x] = data[p];
				if (blockMap[y][x]==1)
					blockMap[y][x]=WL_WALL;
				else if (blockMap[y][x]==2)
					blockMap[y][x]=WL_DESTROYALL;
				else if (blockMap[y][x]==3)
					blockMap[y][x]=WL_ALLOWLIQUID;
				else if (blockMap[y][x]==4)
					blockMap[y][x]=WL_FAN;
				else if (blockMap[y][x]==5)
					blockMap[y][x]=WL_STREAM;
				else if (blockMap[y][x]==6)
					blockMap[y][x]=WL_DETECT;
				else if (blockMap[y][x]==7)
					blockMap[y][x]=WL_EWALL;
				else if (blockMap[y][x]==8)
					blockMap[y][x]=WL_WALLELEC;
				else if (blockMap[y][x]==9)
					blockMap[y][x]=WL_ALLOWAIR;
				else if (blockMap[y][x]==10)
					blockMap[y][x]=WL_ALLOWPOWDER;
				else if (blockMap[y][x]==11)
					blockMap[y][x]=WL_ALLOWALLELEC;
				else if (blockMap[y][x]==12)
					blockMap[y][x]=WL_EHOLE;
				else if (blockMap[y][x]==13)
					blockMap[y][x]=WL_ALLOWGAS;

				if (ver>=44)
				{
					/* The numbers used to save walls were changed, starting in v44.
					 * The new numbers are ignored for older versions due to some corruption of bmap in saves from older versions.
					 */
					if (blockMap[y][x]==O_WL_WALLELEC)
						blockMap[y][x]=WL_WALLELEC;
					else if (blockMap[y][x]==O_WL_EWALL)
						blockMap[y][x]=WL_EWALL;
					else if (blockMap[y][x]==O_WL_DETECT)
						blockMap[y][x]=WL_DETECT;
					else if (blockMap[y][x]==O_WL_STREAM)
						blockMap[y][x]=WL_STREAM;
					else if (blockMap[y][x]==O_WL_FAN||blockMap[y][x]==O_WL_FANHELPER)
						blockMap[y][x]=WL_FAN;
					else if (blockMap[y][x]==O_WL_ALLOWLIQUID)
						blockMap[y][x]=WL_ALLOWLIQUID;
					else if (blockMap[y][x]==O_WL_DESTROYALL)
						blockMap[y][x]=WL_DESTROYALL;
					else if (blockMap[y][x]==O_WL_ERASE)
						blockMap[y][x]=WL_ERASE;
					else if (blockMap[y][x]==O_WL_WALL)
						blockMap[y][x]=WL_WALL;
					else if (blockMap[y][x]==O_WL_ALLOWAIR)
						blockMap[y][x]=WL_ALLOWAIR;
					else if (blockMap[y][x]==O_WL_ALLOWSOLID)
						blockMap[y][x]=WL_ALLOWPOWDER;
					else if (blockMap[y][x]==O_WL_ALLOWALLELEC)
						blockMap[y][x]=WL_ALLOWALLELEC;
					else if (blockMap[y][x]==O_WL_EHOLE)
						blockMap[y][x]=WL_EHOLE;
					else if (blockMap[y][x]==O_WL_ALLOWGAS)
						blockMap[y][x]=WL_ALLOWGAS;
					else if (blockMap[y][x]==O_WL_GRAV)
						blockMap[y][x]=WL_GRAV;
					else if (blockMap[y][x]==O_WL_ALLOWENERGY)
						blockMap[y][x]=WL_ALLOWENERGY;
				}

				if (blockMap[y][x] >= UI_WALLCOUNT)
					blockMap[y][x] = 0;
			}

			p++;
		}
	for (y=by0; y<by0+bh; y++)
		for (x=bx0; x<bx0+bw; x++)
			if (data[(y-by0)*bw+(x-bx0)]==4||(ver>=44 && data[(y-by0)*bw+(x-bx0)]==O_WL_FAN))
			{
				if (p >= dataLength)
					throw ParseException(ParseException::Corrupt, "Not enough data at line " MTOS(__LINE__) " in " MTOS(__FILE__));
				fanVelX[y][x] = (data[p++]-127.0f)/64.0f;
			}
	for (y=by0; y<by0+bh; y++)
		for (x=bx0; x<bx0+bw; x++)
			if (data[(y-by0)*bw+(x-bx0)]==4||(ver>=44 && data[(y-by0)*bw+(x-bx0)]==O_WL_FAN))
			{
				if (p >= dataLength)
					throw ParseException(ParseException::Corrupt, "Not enough data at line " MTOS(__LINE__) " in " MTOS(__FILE__));
				fanVelY[y][x] = (data[p++]-127.0f)/64.0f;
			}

	// load the particle map
	int i = 0;
	k = 0;
	pty = p;
	for (y=y0; y<y0+h; y++)
		for (x=x0; x<x0+w; x++)
		{
			if (p >= dataLength)
				throw ParseException(ParseException::Corrupt, "Not enough data at line " MTOS(__LINE__) " in " MTOS(__FILE__));
			j=data[p++];
			if (j >= PT_NUM) {
				j = PT_DUST;//throw ParseException(ParseException::Corrupt, "Not enough data at line " MTOS(__LINE__) " in " MTOS(__FILE__));
			}
			if (j)
			{
				memset(particles+k, 0, sizeof(Particle));
				particles[k].type = j;
				if (j == PT_COAL)
					particles[k].tmp = 50;
				if (j == PT_FUSE)
					particles[k].tmp = 50;
				if (j == PT_PHOT)
					particles[k].ctype = 0x3fffffff;
				if (j == PT_SOAP)
					particles[k].ctype = 0;
				if (j==PT_BIZR || j==PT_BIZRG || j==PT_BIZRS)
					particles[k].ctype = 0x47FFFF;
				particles[k].x = (float)x;
				particles[k].y = (float)y;
				particleIDMap[(x-x0)+(y-y0)*w] = k+1;
				particlesCount = ++k;
			}
		}

	// load particle properties
	for (j=0; j<w*h; j++)
	{
		i = particleIDMap[j];
		if (i)
		{
			i--;
			if (p+1 >= dataLength)
				throw ParseException(ParseException::Corrupt, "Not enough data at line " MTOS(__LINE__) " in " MTOS(__FILE__));
			if (i < NPART)
			{
				particles[i].vx = (data[p++]-127.0f)/16.0f;
				particles[i].vy = (data[p++]-127.0f)/16.0f;
			}
			else
				p += 2;
		}
	}
	for (j=0; j<w*h; j++)
	{
		i = particleIDMap[j];
		if (i)
		{
			if (ver>=44) {
				if (p >= dataLength) {
					throw ParseException(ParseException::Corrupt, "Not enough data at line " MTOS(__LINE__) " in " MTOS(__FILE__));
				}
				if (i <= NPART) {
					ttv = (data[p++])<<8;
					ttv |= (data[p++]);
					particles[i-1].life = ttv;
				} else {
					p+=2;
				}
			} else {
				if (p >= dataLength)
					throw ParseException(ParseException::Corrupt, "Not enough data at line " MTOS(__LINE__) " in " MTOS(__FILE__));
				if (i <= NPART)
					particles[i-1].life = data[p++]*4;
				else
					p++;
			}
		}
	}
	if (ver>=44) {
		for (j=0; j<w*h; j++)
		{
			i = particleIDMap[j];
			if (i)
			{
				if (p >= dataLength) {
					throw ParseException(ParseException::Corrupt, "Not enough data at line " MTOS(__LINE__) " in " MTOS(__FILE__));
				}
				if (i <= NPART) {
					ttv = (data[p++])<<8;
					ttv |= (data[p++]);
					particles[i-1].tmp = ttv;
					if (ver<53 && !particles[i-1].tmp)
						for (q = 1; q<=NGOL; q++) {
							if (particles[i-1].type==goltype[q-1] && grule[q][9]==2)
								particles[i-1].tmp = grule[q][9]-1;
						}
					if (ver>=51 && ver<53 && particles[i-1].type==PT_PBCN)
					{
						particles[i-1].tmp2 = particles[i-1].tmp;
						particles[i-1].tmp = 0;
					}
				} else {
					p+=2;
				}
			}
		}
	}
	if (ver>=53) {
		for (j=0; j<w*h; j++)
		{
			i = particleIDMap[j];
			ty = data[pty+j];
			if (i && (ty==PT_PBCN || (ty==PT_TRON && ver>=77)))
			{
				if (p >= dataLength)
					throw ParseException(ParseException::Corrupt, "Not enough data at line " MTOS(__LINE__) " in " MTOS(__FILE__));
				if (i <= NPART)
					particles[i-1].tmp2 = data[p++];
				else
					p++;
			}
		}
	}
	//Read ALPHA component
	for (j=0; j<w*h; j++)
	{
		i = particleIDMap[j];
		if (i)
		{
			if (ver>=49) {
				if (p >= dataLength) {
					throw ParseException(ParseException::Corrupt, "Not enough data at line " MTOS(__LINE__) " in " MTOS(__FILE__));
				}
				if (i <= NPART) {
					particles[i-1].dcolour = data[p++]<<24;
				} else {
					p++;
				}
			}
		}
	}
	//Read RED component
	for (j=0; j<w*h; j++)
	{
		i = particleIDMap[j];
		if (i)
		{
			if (ver>=49) {
				if (p >= dataLength) {
					throw ParseException(ParseException::Corrupt, "Not enough data at line " MTOS(__LINE__) " in " MTOS(__FILE__));
				}
				if (i <= NPART) {
					particles[i-1].dcolour |= data[p++]<<16;
				} else {
					p++;
				}
			}
		}
	}
	//Read GREEN component
	for (j=0; j<w*h; j++)
	{
		i = particleIDMap[j];
		if (i)
		{
			if (ver>=49) {
				if (p >= dataLength) {
					throw ParseException(ParseException::Corrupt, "Not enough data at line " MTOS(__LINE__) " in " MTOS(__FILE__));
				}
				if (i <= NPART) {
					particles[i-1].dcolour |= data[p++]<<8;
				} else {
					p++;
				}
			}
		}
	}
	//Read BLUE component
	for (j=0; j<w*h; j++)
	{
		i = particleIDMap[j];
		if (i)
		{
			if (ver>=49) {
				if (p >= dataLength) {
					throw ParseException(ParseException::Corrupt, "Not enough data at line " MTOS(__LINE__) " in " MTOS(__FILE__));
				}
				if (i <= NPART) {
					particles[i-1].dcolour |= data[p++];
				} else {
					p++;
				}
			}
		}
	}
	for (j=0; j<w*h; j++)
	{
		i = particleIDMap[j];
		ty = data[pty+j];
		if (i)
		{
			if (ver>=34&&legacy_beta==0)
			{
				if (p >= dataLength)
				{
					throw ParseException(ParseException::Corrupt, "Not enough data at line " MTOS(__LINE__) " in " MTOS(__FILE__));
				}
				if (i <= NPART)
				{
					if (ver>=42) {
						if (new_format) {
							ttv = (data[p++])<<8;
							ttv |= (data[p++]);
							if (particles[i-1].type==PT_PUMP) {
								particles[i-1].temp = ttv + 0.15;//fix PUMP saved at 0, so that it loads at 0.
							} else {
								particles[i-1].temp = ttv;
							}
						} else {
							particles[i-1].temp = (data[p++]*((MAX_TEMP+(-MIN_TEMP))/255))+MIN_TEMP;
						}
					} else {
						particles[i-1].temp = ((data[p++]*((O_MAX_TEMP+(-O_MIN_TEMP))/255))+O_MIN_TEMP)+273;
					}
				}
				else
				{
					p++;
					if (new_format) {
						p++;
					}
				}
			}
			else
			{
				particles[i-1].temp = elements[particles[i-1].type].Temperature;
			}
		}
	}
	for (j=0; j<w*h; j++)
	{
		int gnum = 0;
		i = particleIDMap[j];
		ty = data[pty+j];
		if (i && (ty==PT_CLNE || (ty==PT_PCLN && ver>=43) || (ty==PT_BCLN && ver>=44) || (ty==PT_SPRK && ver>=21) || (ty==PT_LAVA && ver>=34) || (ty==PT_PIPE && ver>=43) || (ty==PT_LIFE && ver>=51) || (ty==PT_PBCN && ver>=52) || (ty==PT_WIRE && ver>=55) || (ty==PT_STOR && ver>=59) || (ty==PT_CONV && ver>=60)))
		{
			if (p >= dataLength)
				throw ParseException(ParseException::Corrupt, "Not enough data at line " MTOS(__LINE__) " in " MTOS(__FILE__));
			if (i <= NPART)
				particles[i-1].ctype = data[p++];
			else
				p++;
		}
		// no more particle properties to load, so we can change type here without messing up loading
		if (i && i<=NPART)
		{
			if (ver<90 && particles[i-1].type == PT_PHOT)
			{
				particles[i-1].flags |= FLAG_PHOTDECO;
			}
			if (ver<79 && particles[i-1].type == PT_SPNG)
			{
				if (fabs(particles[i-1].vx)>0.0f || fabs(particles[i-1].vy)>0.0f)
					particles[i-1].flags |= FLAG_MOVABLE;
			}

			if (ver<48 && (ty==OLD_PT_WIND || (ty==PT_BRAY&&particles[i-1].life==0)))
			{
				// Replace invisible particles with something sensible and add decoration to hide it
				x = (int)(particles[i-1].x+0.5f);
				y = (int)(particles[i-1].y+0.5f);
				particles[i-1].dcolour = 0xFF000000;
				particles[i-1].type = PT_DMND;
			}
			if(ver<51 && ((ty>=78 && ty<=89) || (ty>=134 && ty<=146 && ty!=141))){
				//Replace old GOL
				particles[i-1].type = PT_LIFE;
				for (gnum = 0; gnum<NGOL; gnum++){
					if (ty==goltype[gnum])
						particles[i-1].ctype = gnum;
				}
				ty = PT_LIFE;
			}
			if(ver<52 && (ty==PT_CLNE || ty==PT_PCLN || ty==PT_BCLN)){
				//Replace old GOL ctypes in clone
				for (gnum = 0; gnum<NGOL; gnum++){
					if (particles[i-1].ctype==goltype[gnum])
					{
						particles[i-1].ctype = PT_LIFE;
						particles[i-1].tmp = gnum;
					}
				}
			}
			if(ty==PT_LCRY){
				if(ver<67)
				{
					//New LCRY uses TMP not life
					if(particles[i-1].life>=10)
					{
						particles[i-1].life = 10;
						particles[i-1].tmp2 = 10;
						particles[i-1].tmp = 3;
					}
					else if(particles[i-1].life<=0)
					{
						particles[i-1].life = 0;
						particles[i-1].tmp2 = 0;
						particles[i-1].tmp = 0;
					}
					else if(particles[i-1].life < 10 && particles[i-1].life > 0)
					{
						particles[i-1].tmp = 1;
					}
				}
				else
				{
					particles[i-1].tmp2 = particles[i-1].life;
				}
			}

			if (ver<81)
			{
				if (particles[i-1].type==PT_BOMB && particles[i-1].tmp!=0)
				{
					particles[i-1].type = PT_EMBR;
					particles[i-1].ctype = 0;
					if (particles[i-1].tmp==1)
						particles[i-1].tmp = 0;
				}
				if (particles[i-1].type==PT_DUST && particles[i-1].life>0)
				{
					particles[i-1].type = PT_EMBR;
					particles[i-1].ctype = (particles[i-1].tmp2<<16) | (particles[i-1].tmp<<8) | particles[i-1].ctype;
					particles[i-1].tmp = 1;
				}
				if (particles[i-1].type==PT_FIRW && particles[i-1].tmp>=2)
				{
					int caddress = restrict_flt(restrict_flt((float)(particles[i-1].tmp-4), 0.0f, 200.0f)*3, 0.0f, (200.0f*3)-3);
					particles[i-1].type = PT_EMBR;
					particles[i-1].tmp = 1;
					particles[i-1].ctype = (((firw_data[caddress]))<<16) | (((firw_data[caddress+1]))<<8) | ((firw_data[caddress+2]));
				}
			}
			if (ver < 89)
			{
				if (particles[i-1].type == PT_FILT)
				{
					if (particles[i-1].tmp<0 || particles[i-1].tmp>3)
						particles[i-1].tmp = 6;
					particles[i-1].ctype = 0;
				}
				else if (particles[i-1].type == PT_QRTZ || particles[i-1].type == PT_PQRT)
				{
					particles[i-1].tmp2 = particles[i-1].tmp;
					particles[i-1].tmp = particles[i-1].ctype;
					particles[i-1].ctype = 0;
				}
			}
			if (ver < 93)
			{
				if (particles[i-1].type == PT_PIPE || particles[i-1].type == PT_PPIP)
				{
					if (particles[i-1].ctype == 1)
						particles[i-1].tmp |= 0x00020000; //PFLAG_INITIALIZING
					particles[i-1].tmp |= (particles[i-1].ctype-1)<<18;
					particles[i-1].ctype = particles[i-1].tmp&0xFF;
				}
				else if (particles[i-1].type == PT_HSWC || particles[i-1].type == PT_PUMP)
				{
					particles[i-1].tmp = 0;
				}
			}
		}
	}

	if (p >= dataLength)
		throw ParseException(ParseException::Corrupt, "Ran past data buffer");

	j = data[p++];
	for (i=0; i<j; i++)
	{
		if (p+6 > dataLength)
			throw ParseException(ParseException::Corrupt, "Not enough data at line " MTOS(__LINE__) " in " MTOS(__FILE__));
		x = data[p++];
		x |= ((unsigned)data[p++])<<8;
		tempSign.x = x+x0;
		x = data[p++];
		x |= ((unsigned)data[p++])<<8;
		tempSign.y = x+y0;
		x = data[p++];
		tempSign.ju = (sign::Justification)x;
		x = data[p++];
		if (p+x > dataLength)
			throw ParseException(ParseException::Corrupt, "Not enough data at line " MTOS(__LINE__) " in " MTOS(__FILE__));
		if(x>254)
			x = 254;
		memcpy(tempSignText, data+p, x);
		tempSignText[x] = 0;
		tempSign.text = format::CleanString(ByteString(tempSignText).FromUtf8(), true, true, true).Substr(0, 45);
		tempSigns.push_back(tempSign);
		p += x;
	}

	for (size_t i = 0; i < tempSigns.size(); i++)
	{
		if(signs.size() == MAXSIGNS)
			break;
		signs.push_back(tempSigns[i]);
	}
}

// restrict the minimum version this save can be opened with
#define RESTRICTVERSION(major, minor) if ((major) > minimumMajorVersion || (((major) == minimumMajorVersion && (minor) > minimumMinorVersion))) {\
	minimumMajorVersion = major;\
	minimumMinorVersion = minor;\
}

// restrict the minimum version this save can be rendered with
#define RESTRICTRENDERVERSION(major, minor) if ((major) > blameSimon_major || (((major) == blameSimon_major && (minor) > blameSimon_minor))) {\
	blameSimon_major = major;\
	blameSimon_minor = minor;\
}

char * GameSave::serialiseOPS(unsigned int & dataLength)
{
	return NULL;
}

void GameSave::ConvertBsonToJson(bson_iterator *iter, Json::Value *j, int depth)
{
	bson_iterator subiter;
	bson_iterator_subiterator(iter, &subiter);
	while (bson_iterator_next(&subiter))
	{
		ByteString key = bson_iterator_key(&subiter);
		if (bson_iterator_type(&subiter) == BSON_STRING)
			(*j)[key] = bson_iterator_string(&subiter);
		else if (bson_iterator_type(&subiter) == BSON_BOOL)
			(*j)[key] = bson_iterator_bool(&subiter);
		else if (bson_iterator_type(&subiter) == BSON_INT)
			(*j)[key] = bson_iterator_int(&subiter);
		else if (bson_iterator_type(&subiter) == BSON_LONG)
			(*j)[key] = (Json::Value::UInt64)bson_iterator_long(&subiter);
		else if (bson_iterator_type(&subiter) == BSON_ARRAY && depth < 5)
		{
			bson_iterator arrayiter;
			bson_iterator_subiterator(&subiter, &arrayiter);
			int length = 0, length2 = 0;
			while (bson_iterator_next(&arrayiter))
			{
				if (bson_iterator_type(&arrayiter) == BSON_OBJECT && !strcmp(bson_iterator_key(&arrayiter), "part"))
				{
					Json::Value tempPart;
					ConvertBsonToJson(&arrayiter, &tempPart, depth + 1);
					(*j)["links"].append(tempPart);
					length++;
				}
				else if (bson_iterator_type(&arrayiter) == BSON_INT && !strcmp(bson_iterator_key(&arrayiter), "saveID"))
				{
					(*j)["links"].append(bson_iterator_int(&arrayiter));
				}
				length2++;
				if (length > (int)(40 / ((depth+1) * (depth+1))) || length2 > 50)
					break;
			}
		}
	}
}

std::set<int> GetNestedSaveIDs(Json::Value j)
{
	Json::Value::Members members = j.getMemberNames();
	std::set<int> saveIDs = std::set<int>();
	for (Json::Value::Members::iterator iter = members.begin(), end = members.end(); iter != end; ++iter)
	{
		ByteString member = *iter;
		if (member == "id" && j[member].isInt())
			saveIDs.insert(j[member].asInt());
		else if (j[member].isArray())
		{
			for (Json::Value::ArrayIndex i = 0; i < j[member].size(); i++)
			{
				// only supports objects and ints here because that is all we need
				if (j[member][i].isInt())
				{
					saveIDs.insert(j[member][i].asInt());
					continue;
				}
				if (!j[member][i].isObject())
					continue;
				std::set<int> nestedSaveIDs = GetNestedSaveIDs(j[member][i]);
				saveIDs.insert(nestedSaveIDs.begin(), nestedSaveIDs.end());
			}
		}
	}
	return saveIDs;
}

// converts a json object to bson
void GameSave::ConvertJsonToBson(bson *b, Json::Value j, int depth)
{
	Json::Value::Members members = j.getMemberNames();
	for (Json::Value::Members::iterator iter = members.begin(), end = members.end(); iter != end; ++iter)
	{
		ByteString member = *iter;
		if (j[member].isString())
			bson_append_string(b, member.c_str(), j[member].asCString());
		else if (j[member].isBool())
			bson_append_bool(b, member.c_str(), j[member].asBool());
		else if (j[member].type() == Json::intValue)
			bson_append_int(b, member.c_str(), j[member].asInt());
		else if (j[member].type() == Json::uintValue)
			bson_append_long(b, member.c_str(), j[member].asInt64());
		else if (j[member].isArray())
		{
			bson_append_start_array(b, member.c_str());
			std::set<int> saveIDs = std::set<int>();
			int length = 0;
			for (Json::Value::ArrayIndex i = 0; i < j[member].size(); i++)
			{
				// only supports objects and ints here because that is all we need
				if (j[member][i].isInt())
				{
					saveIDs.insert(j[member][i].asInt());
					continue;
				}
				if (!j[member][i].isObject())
					continue;
				if (depth > 4 || length > (int)(40 / ((depth+1) * (depth+1))))
				{
					std::set<int> nestedSaveIDs = GetNestedSaveIDs(j[member][i]);
					saveIDs.insert(nestedSaveIDs.begin(), nestedSaveIDs.end());
				}
				else
				{
					bson_append_start_object(b, "part");
					ConvertJsonToBson(b, j[member][i], depth+1);
					bson_append_finish_object(b);
				}
				length++;
			}
			for (std::set<int>::iterator iter = saveIDs.begin(), end = saveIDs.end(); iter != end; ++iter)
			{
				bson_append_int(b, "saveID", *iter);
			}
			bson_append_finish_array(b);
		}
	}
}

// deallocates a pointer to a 2D array and sets it to NULL
template <typename T>
void GameSave::Deallocate2DArray(T ***array, int blockHeight)
{
	if (*array)
	{
		for (int y = 0; y < blockHeight; y++)
			delete[] (*array)[y];
		delete[] (*array);
		*array = NULL;
	}
}

bool GameSave::TypeInCtype(int type, int ctype)
{
	return ctype >= 0 && ctype < PT_NUM &&
	        (type == PT_CLNE || type == PT_PCLN || type == PT_BCLN || type == PT_PBCN ||
	        type == PT_STOR || type == PT_CONV || type == PT_STKM || type == PT_STKM2 ||
	        type == PT_FIGH || type == PT_LAVA || type == PT_SPRK || type == PT_PSTN ||
	        type == PT_CRAY || type == PT_DTEC || type == PT_DRAY || type == PT_PIPE ||
	        type == PT_PPIP || type == PT_LDTC);
}

bool GameSave::TypeInTmp(int type)
{
	return type == PT_STOR;
}

bool GameSave::TypeInTmp2(int type, int tmp2)
{
	return false;
}

void GameSave::dealloc()
{
	if (particles)
	{
		delete[] particles;
		particles = NULL;
	}
	Deallocate2DArray<unsigned char>(&blockMap, blockHeight);
	Deallocate2DArray<float>(&fanVelX, blockHeight);
	Deallocate2DArray<float>(&fanVelY, blockHeight);
	Deallocate2DArray<float>(&pressure, blockHeight);
	Deallocate2DArray<float>(&velocityX, blockHeight);
	Deallocate2DArray<float>(&velocityY, blockHeight);
	Deallocate2DArray<float>(&ambientHeat, blockHeight);
}

GameSave::~GameSave()
{
	dealloc();
}
