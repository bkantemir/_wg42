#include "GameSubj.h"
#include "platform.h"
#include "utils.h"
#include "TheGame.h"
#include "DrawJob.h"
#include "MaterialAdjust.h"
#include "Shadows.h"
#include "Shader.h"
#include "ModelLoaderCmd.h"

extern TheGame theGame;
extern float degrees2radians;

GameSubj::GameSubj() {
}
GameSubj::~GameSubj() {
    if (pCustomColors != NULL) {
        int totalN = pCustomColors->size();
        for (int i = 0; i < totalN; i++)
            delete pCustomColors->at(i);
        pCustomColors->clear();
        delete pCustomColors;
        pCustomColors = NULL;
    }
    if (pCustomMaterials != NULL) {
        int totalN = pCustomMaterials->size();
        for (int i = 0; i < totalN; i++)
            delete pCustomMaterials->at(i);
        pCustomMaterials->clear();
        delete pCustomMaterials;
        pCustomMaterials = NULL;
    }
}
void GameSubj::buildModelMatrix(GameSubj* pGS) {
    mat4x4_translate(pGS->ownModelMatrixUnscaled, pGS->ownCoords.pos[0], pGS->ownCoords.pos[1], pGS->ownCoords.pos[2]);
    //rotation order: Z-X-Y
    mat4x4_mul(pGS->ownModelMatrixUnscaled, pGS->ownModelMatrixUnscaled, pGS->ownCoords.rotationMatrix);

    if(pGS->d2parent != 0) {
        GameSubj* pParent = pGS->pSubjsSet->at(pGS->nInSubjsSet - pGS->d2parent);
        mat4x4_mul(pGS->absModelMatrixUnscaled, pParent->absModelMatrixUnscaled, pGS->ownModelMatrixUnscaled);
    }
    else if(pGS->pStickTo != NULL)
        mat4x4_mul(pGS->absModelMatrixUnscaled, pGS->pStickTo->absModelMatrixUnscaled, pGS->ownModelMatrixUnscaled);
    else
        memcpy(pGS->absModelMatrixUnscaled, pGS->ownModelMatrixUnscaled, sizeof(mat4x4));
        
    if (v3equals(pGS->scale, 1))
        memcpy(pGS->absModelMatrix, pGS->absModelMatrixUnscaled, sizeof(mat4x4));
    else
        mat4x4_scale_aniso(pGS->absModelMatrix, pGS->absModelMatrixUnscaled, pGS->scale[0], pGS->scale[1], pGS->scale[2]);

    //update absCoords
    if (pGS->d2parent == 0)
        memcpy(&pGS->absCoords, &pGS->ownCoords, sizeof(Coords));
    else {
        Coords::getPositionFromMatrix(pGS->absCoords.pos, pGS->absModelMatrixUnscaled);
    }
}
int GameSubj::applySpeeds(GameSubj* pGS) {
    bool angleChanged = false;
    for(int i=0;i<3;i++)
        if (pGS->ownSpeed.eulerDg[i] != 0) {
            angleChanged = true;
            pGS->ownCoords.eulerDg[i] += pGS->ownSpeed.eulerDg[i];
            if (pGS->ownCoords.eulerDg[i] > 180.0)
                pGS->ownCoords.eulerDg[i] -= 360.0;
            else if (pGS->ownCoords.eulerDg[i] <= -180.0)
                pGS->ownCoords.eulerDg[i] += 360.0;
        }
    if(angleChanged)
        Coords::eulerDgToMatrix(pGS->ownCoords.rotationMatrix, pGS->ownCoords.eulerDg);
    return 1;
}

int GameSubj::renderStandard(GameSubj* pGS, mat4x4 mViewProjection, Camera* pCam, float* dirToMainLight) {
    //build mMVP4dm (shadow MVP) matrix for given subject
    mat4x4 mMVP4dm;
    mat4x4_mul(mMVP4dm, Shadows::mViewProjection, pGS->absModelMatrix);

    //build MVP matrix for given subject
    mat4x4 mMVP;
    mat4x4_mul(mMVP, mViewProjection, pGS->absModelMatrix);
    //build Model-View (rotation) matrix for normals
    mat4x4 mMV4x4;
    mat4x4_mul(mMV4x4, pCam->lookAtMatrix, pGS->absModelMatrixUnscaled);
    //convert to 3x3 matrix
    float mMV3x3[3][3];
    for (int y = 0; y < 3; y++)
        for (int x = 0; x < 3; x++)
            mMV3x3[y][x] = mMV4x4[y][x];
    //subj's distance from camera
    float cameraSpacePos[4];
    mat4x4_mul_vec4plus(cameraSpacePos, pCam->lookAtMatrix, pGS->absCoords.pos, 1);
    float zDistance = abs(cameraSpacePos[2]);
    float cotangentA = 1.0f / tanf(degrees2radians * pCam->viewRangeDg / 2.0f);
    float halfScreenVertSizeInUnits = zDistance / cotangentA;
    float sizeUnitPixelsSize = (float)theGame.screenSize[1] / 2.0f / halfScreenVertSizeInUnits;
    sizeUnitPixelsSize *= pGS->scale[0];
    //render subject
    for (int i = 0; i < pGS->djTotalN; i++) {
        DrawJob* pDJ = DrawJob::drawJobs.at(pGS->djStartN + i);
        Material* pMt = &pDJ->mt;
        Material* pMt2 = &pDJ->mtLayer2;
        Material* pAltMt = NULL;
        Material* pAltMt2 = NULL;
        if (i == 0 && pGS->mt0isSet > 0) //already customized
            pMt = &pGS->mt0;
        else{ //customize?
            GameSubj* pRoot = pGS->pSubjsSet->at(pGS->rootN);
            if (pRoot->pCustomMaterials != NULL) {
                int customMaterialsN = pRoot->pCustomMaterials->size();
                if (strlen(pMt->materialName) != 0){
                    for (int oN = 0; oN <= customMaterialsN; oN++) {
                        if (oN == customMaterialsN) {
                            myStrcpy_s(pMt->materialName, 32, "");
                            break;
                        }
                        MaterialAdjust* pMA = pRoot->pCustomMaterials->at(oN); //custom color option
                        if (strcmp(pMA->materialName, pMt->materialName) == 0) {
                            pAltMt = new Material(*pMt);
                            pMt = pAltMt;
                            pMA->adjust(pMt, pMA);
                            pMt->pickShaderNumber(pMt);
                            //2-nd layer
                            if (strlen(pMt->layer2as) == 0)
                                pMt2 = NULL;
                            else if(strcmp(pMt->layer2as,pMt2->materialName) != 0) {
                                pAltMt2 = new Material(*pMt);
                                pMt2 = pAltMt2;
                                pMt2->shaderN = -1;
                                myStrcpy_s(pMt2->materialName, 32, pMt->layer2as);
                                //look in
                                std::vector<MaterialAdjust*>* pMAlist = &MaterialAdjust::materialAdjustsList;
                                customMaterialsN = pMAlist->size();
                                for (int oN = 0; oN < customMaterialsN; oN++) {
                                    MaterialAdjust* pMA = pMAlist->at(oN); //custom color option
                                    if (strcmp(pMA->materialName, pMt2->materialName) == 0) {
                                        pMA->adjust(pMt2, pMA);
                                        pMt2->pickShaderNumber(pMt2);
                                    }
                                }
                            }
                            break;
                        }
                    }
                }
            }

            if (pRoot->pCustomColors != NULL) {
                int customColorsN = pRoot->pCustomColors->size();
                //customize colors?
                MyColor* pColors[3];
                pColors[0] = &pMt->uColor;
                pColors[1] = &pMt->uColor1;
                pColors[2] = &pMt->uColor2;
                for (int cN = 0; cN < 3; cN++) {
                    MyColor* pCL = pColors[cN];
                    if (strlen(pCL->colorName) == 0)
                        continue;
                    for (int oN = 0; oN <= customColorsN; oN++) {
                        if (oN == customColorsN) {
                            myStrcpy_s(pCL->colorName, 32, "");
                            break;
                        }
                        MyColor* pCC = pRoot->pCustomColors->at(oN); //custom color option
                        if (strcmp(pCC->colorName, pCL->colorName) == 0) {
                            memcpy(pCL, pCC, sizeof(MyColor));
                            break;
                        }
                    }
                }
            }
            if (i == 0) {
                memcpy(&pGS->mt0, pMt, sizeof(Material));
                pGS->mt0isSet = 1;
            }
        }
        pDJ->execute((float*)mMVP, *mMV3x3, (float*)pGS->absModelMatrix, (float*)mMVP4dm, dirToMainLight, pCam->ownCoords.pos,
            sizeUnitPixelsSize, pMt);
        //have 2-nd layer ?
        if (pMt2 != NULL)
        if (pMt2->shaderN >= 0) {
            pDJ->execute((float*)mMVP, *mMV3x3, (float*)pGS->absModelMatrix, (float*)mMVP4dm, dirToMainLight, pCam->ownCoords.pos,
                sizeUnitPixelsSize, pMt2);
        }
        if (pAltMt != NULL) {
            delete pAltMt;
            pAltMt = NULL;
        }
        if (pAltMt2 != NULL) {
            delete pAltMt2;
            pAltMt2 = NULL;
        }
    }
    return 1;
}
int GameSubj::renderDepthMap(GameSubj* pGS, mat4x4 mViewProjection, Camera* pCam) {
    if (pGS->dropsShadow < 1)
        return 0;
    mat4x4 mMVP;
    mat4x4_mul(mMVP, mViewProjection, pGS->absModelMatrix);
    mat4x4 mMV4x4;
    mat4x4_mul(mMV4x4, pCam->lookAtMatrix, pGS->absModelMatrixUnscaled);
    //convert to 3x3 matrix
    float mMV3x3[3][3];
    for (int y = 0; y < 3; y++)
        for (int x = 0; x < 3; x++)
            mMV3x3[y][x] = mMV4x4[y][x];
    //render subject
    for (int i = 0; i < pGS->djTotalN; i++) {
        DrawJob* pDJ = DrawJob::drawJobs.at(pGS->djStartN + i);
        if (pDJ->mt.dropsShadow < 1)
            continue;
        Material* pMT = &pDJ->mtShadow;
                    //Shader* pSh = Shader::shaders.at(pMT->shaderN);

        pDJ->executeDrawJob(pDJ, (float*)mMVP, *mMV3x3, NULL, NULL, NULL, NULL,
            Shadows::sizeUnitPixelsSize*pGS->scale[0], pMT);
    }
    return 1;
}
int GameSubj::scaleStandard(GameSubj* pGS, float k) {
    for (int i = 0; i < 3; i++)
        pGS->scale[i] *= k;
    int subjsN = pGS->pSubjsSet->size();
    for (int sN = pGS->nInSubjsSet + 1; sN < subjsN; sN++) {
        GameSubj* pGS2 = pGS->pSubjsSet->at(sN);
        if (pGS2 == NULL)
            continue;
        if (pGS2->nInSubjsSet - pGS2->d2parent != pGS->nInSubjsSet)
            continue;
        for (int i = 0; i < 3; i++)
            pGS2->ownCoords.pos[i] *= k;
        pGS2->scaleMe(k);
    }
    return 1;
}
int GameSubj::onDeployStandatd(GameSubj* pGS) {
    if (strlen(pGS->fileOnDeploy) == 0)
        return 0;
    std::string fileOnDeployStr(pGS->fileOnDeploy);
    ModelLoaderCmd* pML = new ModelLoaderCmd(pGS, fileOnDeployStr);
    pML->processSource(pML);
    delete pML;
    return 1;
}
int GameSubj::setRoot(GameSubj* pGS00) {
    int rootN = pGS00->nInSubjsSet;
    std::vector<GameSubj*> subjs2check;
    subjs2check.push_back(pGS00);
    int totalN = pGS00->pSubjsSet->size();
    while (subjs2check.size() > 0) {
        GameSubj* pParent = subjs2check.back();
        subjs2check.pop_back();
        pParent->rootN = rootN;
        int parentN = pParent->nInSubjsSet;
        for (int gsN = parentN + 1; gsN < totalN; gsN++) {            
            GameSubj* pChild = pParent->pSubjsSet->at(gsN);
            if (pChild->nInSubjsSet - pChild->d2parent != parentN)
                continue;
            subjs2check.push_back(pChild);
        }
    }
    return 1;
}
int GameSubj::processSubj(GameSubj* pGS) {
    //check root
    if (pGS->d2parent == 0)
        pGS->rootN = pGS->nInSubjsSet;
    else {
        int parentN = pGS->nInSubjsSet - pGS->d2parent;
        GameSubj* pParent = pGS->pSubjsSet->at(parentN);
        //inherit parent
        pGS->rootN = pParent->rootN;
        pGS->hide = pParent->hide;
    }

    pGS->moveSubj();
    buildModelMatrix(pGS);
    return 1;
}