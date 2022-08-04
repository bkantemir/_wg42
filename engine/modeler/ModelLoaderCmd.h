#pragma once
#include "XMLparser.h"
#include "GameSubj.h"
#include <map>

class ModelLoaderCmd : public XMLparser
{
public:
	GameSubj* pGS00 = NULL;
	std::vector<GameSubj*>* pSubjsVector = NULL;

	std::vector<MaterialAdjust*>* pTempMaterials = NULL;
	std::vector<MyColor*>* pTempColors = NULL;
	std::map<std::string, std::string> tempVarsHashMap;
public:
	ModelLoaderCmd(GameSubj* pGS0, std::string filePath) : XMLparser(filePath) {
		pGS00 = pGS0;
		pSubjsVector = pGS00->pSubjsSet;
	};
	virtual ~ModelLoaderCmd();
	virtual int processTag() { return processTag(this); };
	static int processTag(ModelLoaderCmd* pML);
	static std::string pickOneOf(std::string optsString);
	static MyColor* findColor(ModelLoaderCmd* pML, const char* colorLabel);
	static MaterialAdjust* findMaterial(ModelLoaderCmd* pML, const char* materialLabel);
};
